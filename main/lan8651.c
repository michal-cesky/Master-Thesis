#include "lan8651.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "main.h"
#include "spi.h"
#include "tc6.h"

static const char *PHY_TAG = "LAN8651";

TC6_t *tc6_instance = NULL;
static bool serviceNeeded = false;
static uint32_t g_chipIdValue = 0;


void initPhyResetPin(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_RESET),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    vTaskDelay(pdMS_TO_TICKS(1000));
    printf("IRQ_PIN level: %d\r\n", gpio_get_level(IRQ_PIN));


    gpio_config(&io_conf);

    gpio_set_level(PIN_NUM_RESET, 0);
    printf("IRQ_PIN level: %d\r\n", gpio_get_level(IRQ_PIN));

    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_NUM_RESET, 1);

    printf("IRQ_PIN level: %d\r\n", gpio_get_level(IRQ_PIN));
    vTaskDelay(pdMS_TO_TICKS(100));

    //vTaskDelay(pdMS_TO_TICKS(1000));
    printf("IRQ_PIN level: %d\r\n", gpio_get_level(IRQ_PIN));

    printf("Try restart\r\n");

    int irqState = gpio_get_level(IRQ_PIN);
    if (irqState == 0) {
        printf("IRQ_N asserted, reset completed.\r\n");
    } else {
        printf("IRQ_N not asserted, reset may have failed.\r\n");
    }
}

void init_tc6(void)
{
    tc6_instance = TC6_Init((void *)spi_handle);
    if (tc6_instance == NULL) {
        ESP_LOGE("TC6", "Failed to initialize TC6!");
        return;
    }

    ESP_LOGI("TC6", "TC6 initialized successfully!");
    
    
    // Inicializace registrů TC6
    bool regs_init_ok = TC6Regs_Init(tc6_instance, NULL, macAddress, 
                                     PLCA_ENABLE, // PLCA enable
                                     NODE_ID,     // nodeId
                                     NODE_COUNT,     // nodeCount
                                     0,     // burstCount
                                     0,     // burstTimer
                                     true, // promiscuous
                                     false, // txCutThrough
                                     false);// rxCutThrough
                                     
    if (!regs_init_ok) {
        ESP_LOGE("TC6", "Failed to initialize TC6 registers!");
    } else {
        ESP_LOGI("TC6", "TC6 registers initialized successfully!");
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    TC6_EnableData(tc6_instance, true);

    vTaskDelay(pdMS_TO_TICKS(100));
}

void SyncTask(void *pvParameters)
{
    TC6_t *tc6_instance = (TC6_t *)pvParameters;

    static uint32_t last_check = 0;


    while (1) {
        // Obsluha TC6
        handle_tc6_service();

        // Volání TC6_Service i když serviceNeeded=false
        TC6_Service(tc6_instance, true);

        // Každých 10 sekund kontrolujeme stav synchronizace
        uint32_t now = esp_log_timestamp();
        if ((now - last_check) > 10000) {
            bool synced;
            uint8_t txCredit, rxCredit;

            TC6_GetState(tc6_instance, &txCredit, &rxCredit, &synced);
            ESP_LOGI("TC6_TASK", "LAN8651 status - Synced: %s, TX Credit: %u, RX Credit: %u",
                     synced ? "YES" : "NO", txCredit, rxCredit);

            last_check = now;
        }

        // Periodické volání TC6Regs_CheckTimers
        TC6Regs_CheckTimers();

        // Krátké zpoždění pro úsporu CPU
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}




// Funkce pro asynchronní čtení z registru, s návratem hodnoty
uint32_t lan8651ReadChipId(TC6_t *tc6_instance)
{
    g_chipIdValue = 0;
    uint32_t address = 0x00000000;

    bool ok = TC6_ReadRegister(tc6_instance, address, false, onReadChipId, NULL);
    if (!ok) {
        ESP_LOGE(TAG, "Could not begin read of Chip ID register!");
        return 0;
    }

    // Zde čekáme, až callback 'onReadChipId' vyplní g_chipIdValue
    while (g_chipIdValue == 0) {
        (void)TC6_Service(tc6_instance, false);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return g_chipIdValue;
}

void printChipRevision(TC6_t *pInst){
    uint8_t rev = TC6Regs_GetChipRevision(pInst);
    ESP_LOGI(TAG, "LAN865x Chip Revision: %u", rev);
}





// Callback volaný po dočtení hodnoty z registru
static void onReadChipId(TC6_t *pInst, bool success, uint32_t addr, uint32_t regValue, void *pTag, void *pGlobalTag)
{
    if (success) {
        g_chipIdValue = regValue;
    }
}

void handle_tc6_service(void)
{
    if (serviceNeeded) {
        serviceNeeded = false; // Resetujeme flag
        TC6_Service(tc6_instance, false);
    }
}

// Callback for indicating that the TC6 service needs to be called
void TC6_CB_OnNeedService(TC6_t *pInst, void *pGlobalTag)
{
    DEBUG_LOG(TAG, "TC6_CB_OnNeedService called");
    // Nastavíme flag, že je potřeba obsloužit TC6
    serviceNeeded = true;
}

// Callback for handling errors
void TC6_CB_OnError(TC6_t *pInst, TC6_Error_t err, void *pGlobalTag)
{
    ESP_LOGE(TAG, "TC6 error occurred: %d", err);
}

// Callback for handling TC6 register events
void TC6Regs_CB_OnEvent(TC6_t *pInst, TC6Regs_Event_t event, void *pTag)
{
    ESP_LOGI(TAG, "TC6 register event: %d", event);
}

// Callback for providing the current time in milliseconds
uint32_t TC6Regs_CB_GetTicksMs(void)
{
    return esp_log_timestamp();
}

static void OnSoftResetCB(TC6_t *pInst, bool success, uint32_t addr, uint32_t value, void *pTag, void *pGlobalTag)
{
    ESP_LOGI("OnSoftResetCB", "Soft reset callback: success=%d, addr=0x%08lX, value=0x%08lX", success, addr, value);
}

static void OnReadId1(TC6_t *pInst, bool success, uint32_t addr, uint32_t value, void *pTag, void *pGlobalTag)
{
    ESP_LOGI("OnReadId1", "Callback: success=%d, addr=0x%08lX, value=0x%08lX", success, addr, value);
}

static void OnReadId2(TC6_t *pInst, bool success, uint32_t addr, uint32_t value, void *pTag, void *pGlobalTag)
{
    ESP_LOGI("OnReadId2", "Callback: success=%d, addr=0x%08lX, value=0x%08lX", success, addr, value);
}