#include "esp_log.h"
#include "driver/gpio.h"

#include "lan8651.h"
#include "configuration.h"
#include "main.h"
#include "spi.h"

static const char *PHY_TAG = "LAN8651";
static const char *TC6_TAG = "TC6";

TC6_t *tc6_instance = NULL;
static bool serviceNeeded = false;
static uint32_t g_chipIdValue = 0;

// Function for reading MAC register
static void onReadMAC(TC6_t *pInst, bool success, uint32_t addr, uint32_t regValue, void *pTag, void *pGlobalTag);

// Helper function to process TC6 service requests when needed
static void HandleTc6Service(void);




void initPhyResetPin(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_RESET),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(PHY_TAG, "IRQ_PIN level: %d", gpio_get_level(IRQ_PIN));

    gpio_config(&io_conf);

    gpio_set_level(PIN_NUM_RESET, 0);
    ESP_LOGI(PHY_TAG, "IRQ_PIN level: %d", gpio_get_level(IRQ_PIN));

    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_NUM_RESET, 1);

    ESP_LOGI(PHY_TAG, "IRQ_PIN level: %d", gpio_get_level(IRQ_PIN));
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(PHY_TAG, "IRQ_PIN level: %d", gpio_get_level(IRQ_PIN));

    int irqState = gpio_get_level(IRQ_PIN);
    if (irqState == 0) {
        ESP_LOGI(PHY_TAG, "IRQ_N asserted, reset completed");
    } else {
        ESP_LOGW(PHY_TAG, "IRQ_N not asserted, reset may have failed");
    }
}

void initTc6(void) {
    ESP_LOGI(TC6_TAG, "Initializing TC6...");

    bool promiscuous_mode = SNIFFER ? true : false;

    tc6_instance = TC6_Init((void *)spi_handle);
    if (tc6_instance == NULL) {
        ESP_LOGE(TC6_TAG, "Failed to initialize TC6!");
        return;
    }

    ESP_LOGI(TC6_TAG, "TC6 initialized successfully!");
    
    // Inicilization of the LAN8651 registers
    bool regs_init_ok = TC6Regs_Init(tc6_instance, NULL, macAddress, 
                                     PLCA_ENABLE, // PLCA enable
                                     NODE_ID,     // nodeId
                                     NODE_COUNT,     // nodeCount
                                     BURST_COUNT,     // burstCount
                                     BURST_TIMER,     // burstTimer
                                     SNIFFER, // promiscuous mode
                                     TX_CUT_THROUHG, // txCutThrough
                                     RX_CUT_THROUGH);// rxCutThrough
                                     
    if (!regs_init_ok) {
        ESP_LOGE(TC6_TAG, "Failed to initialize TC6 registers!");
    } else {
        ESP_LOGI(TC6_TAG, "TC6 registers initialized successfully!");
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    TC6_EnableData(tc6_instance, true);

    vTaskDelay(pdMS_TO_TICKS(100));
}

void SyncTask(void *pvParameters) {
    TC6_t *tc6_instance = (TC6_t *)pvParameters;

    static uint32_t last_check = 0;

    while (1) {
        HandleTc6Service();

        TC6_Service(tc6_instance, true);

        // Every 10 seconds, chack synchronization status of the LAN8651
        uint32_t now = esp_log_timestamp();
        if ((now - last_check) > 10000) {
            bool synced;
            uint8_t txCredit, rxCredit;

            TC6_GetState(tc6_instance, &txCredit, &rxCredit, &synced);
            printf("\n");
            ESP_LOGI(PHY_TAG, "LAN8651 status - Synced: %s, TX Credit: %u, RX Credit: %u\n", synced ? "YES" : "NO", txCredit, rxCredit);

            last_check = now;
        }

        TC6Regs_CheckTimers();

    }
}



void ReadMacControlRegister(TC6_t *tc6_instance) {
    g_chipIdValue = 0;
    uint32_t address = 0x00000000;

    bool ok = TC6_ReadRegister(tc6_instance, address, false, onReadMAC, NULL);
    if (!ok) {
        ESP_LOGE(PHY_TAG, "Could not begin read of Chip ID register!");
    }

    // Wait for callback function to set g_chipIdValue
    while (g_chipIdValue == 0) {
        (void)TC6_Service(tc6_instance, false);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(PHY_TAG, "LAN8651 Chip ID: 0x%08lX", g_chipIdValue);
}

static void onReadMAC(TC6_t *pInst, bool success, uint32_t addr, uint32_t regValue, void *pTag, void *pGlobalTag) {
    if (success) {
        g_chipIdValue = regValue;
    }
}



void HandleTc6Service(void) {
    if (serviceNeeded) {
        serviceNeeded = false;
        TC6_Service(tc6_instance, false);
    }
}

// Callback for indicating that the TC6 service needs to be called
void TC6_CB_OnNeedService(TC6_t *pInst, void *pGlobalTag) {
    serviceNeeded = true;
}

// Callback for handling errors
void TC6_CB_OnError(TC6_t *pInst, TC6_Error_t err, void *pGlobalTag) {
    ESP_LOGE(TC6_TAG, "TC6 error occurred: %d", err);
}

// Callback for providing the current time in milliseconds
uint32_t TC6Regs_CB_GetTicksMs(void) {
    return esp_log_timestamp();
}

// Callback for handling TC6 register events
void TC6Regs_CB_OnEvent(TC6_t *pInst, TC6Regs_Event_t event, void *pTag) {
    ESP_LOGI(TC6_TAG, "TC6 register event: %d", event);
}