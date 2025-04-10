#include "tc6.h"
#include "tc6-regs.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_netif.h"
#include <string.h>
#include <inttypes.h>

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5
#define PIN_NUM_RESET   4

#define IRQ_PIN 21

static const char *TAG = "TC6_MAIN";
static spi_device_handle_t spi_handle;

// Globální proměnná pro TC6 instance
static TC6_t *tc6_instance = NULL;

// Globální proměnná pro dočasné uložení přečtené hodnoty
static volatile uint32_t g_chipIdValue = 0;

// Deklarace prototypů
static esp_err_t init_spi(void);
static void init_tc6(void); // Přidáno
static void handle_tc6_service(void); // Přidáno
static void onReadChipId(TC6_t *pInst, bool success, uint32_t addr, uint32_t regValue, void *pTag, void *pGlobalTag);
static uint32_t lan8651ReadChipId(TC6_t *tc6_instance);
static void printChipRevision(TC6_t *pInst);
void initPhyResetPin(void);
static void OnSoftResetCB(TC6_t *pInst, bool success, uint32_t addr, uint32_t value, void *pTag, void *pGlobalTag);
static void OnReadId1(TC6_t *pInst, bool success, uint32_t addr, uint32_t value, void *pTag, void *pGlobalTag);
static void OnReadId2(TC6_t *pInst, bool success, uint32_t addr, uint32_t value, void *pTag, void *pGlobalTag);
void tc6Task(void *pvParameters);


// Globální proměnná pro signalizaci potřeby obsluhy
static volatile bool serviceNeeded = false;




void app_main(void)
{
    ESP_LOGI("MAIN", "Initializing SPI...");
    if (init_spi() != ESP_OK) {
        ESP_LOGE("MAIN", "SPI initialization failed!");
        return;
    }

    initPhyResetPin();

    ESP_LOGI("MAIN", "Initializing TC6...");
    init_tc6();
    /*
    // Čekání na inicializaci TC6 - pravidelně volat Service
    for (int i = 0; i < 10; i++) {
        TC6_Service(tc6_instance, true);
        vTaskDelay(pdMS_TO_TICKS(100));
    }*/
    
    // Přečtení a výpis Chip ID a revize pro ověření komunikace
    uint32_t chipId = lan8651ReadChipId(tc6_instance);
    ESP_LOGI("MAIN", "LAN8651 Chip ID: 0x%08lX", chipId);
    
    uint8_t chipRev = TC6Regs_GetChipRevision(tc6_instance);
    ESP_LOGI("MAIN", "LAN8651 Chip Revision: %u", chipRev);


  /*  // Hlavní smyčka aplikace - toto je KRITICKÉ pro fungování TC6
    while (1) {
        // Obsluha TC6 - musí být voláno pravidelně v hlavní smyčce
        handle_tc6_service();
        
        // Volání TC6_Service i když serviceNeeded=false (pro jistotu)
        TC6_Service(tc6_instance, true);
        
        // Každých 5 sekund kontrolujeme stav synchronizace
        static uint32_t last_check = 0;
        uint32_t now = esp_log_timestamp();
        
        if ((now - last_check) > 5000) {
            bool synced;
            uint8_t txCredit, rxCredit;
            
            TC6_GetState(tc6_instance, &txCredit, &rxCredit, &synced);
            ESP_LOGI("MAIN", "LAN8651 status - Synced: %s, TX Credit: %u, RX Credit: %u",
                    synced ? "YES" : "NO", txCredit, rxCredit);
            
            last_check = now;
            
            // Kontrola inicializace registrů
            if (TC6Regs_GetInitDone(tc6_instance)) {
                ESP_LOGI("MAIN", "TC6 registers initialization complete");
            } else {
                ESP_LOGW("MAIN", "TC6 registers initialization not complete");
            }
        }
        
        // Periodické volání TC6Regs_CheckTimers
        TC6Regs_CheckTimers();
        
        // Krátké zpoždění pro úsporu CPU
        vTaskDelay(pdMS_TO_TICKS(100));
    }*/
   // Spuštění tasku pro obsluhu TC6
    xTaskCreate(tc6Task, "TC6Task", 4096, (void *)tc6_instance, 5, NULL);

    uint32_t chipId1 = lan8651ReadChipId(tc6_instance);
    ESP_LOGI("MAIN", "LAN8651 Chip ID: 0x%08lX", chipId1);
    
    uint8_t chipRev1 = TC6Regs_GetChipRevision(tc6_instance);
    ESP_LOGI("MAIN", "LAN8651 Chip Revision: %u", chipRev1);


} 

    void tc6Task(void *pvParameters)
{
    TC6_t *tc6_instance = (TC6_t *)pvParameters;

    static uint32_t last_check = 0;

    while (1) {
        // Obsluha TC6 - musí být voláno pravidelně
        handle_tc6_service();

        // Volání TC6_Service i když serviceNeeded=false (pro jistotu)
        TC6_Service(tc6_instance, true);

        // Každých 5 sekund kontrolujeme stav synchronizace
        uint32_t now = esp_log_timestamp();
        if ((now - last_check) > 5000) {
            bool synced;
            uint8_t txCredit, rxCredit;

            TC6_GetState(tc6_instance, &txCredit, &rxCredit, &synced);
            ESP_LOGI("TC6_TASK", "LAN8651 status - Synced: %s, TX Credit: %u, RX Credit: %u",
                     synced ? "YES" : "NO", txCredit, rxCredit);

            last_check = now;

            // Kontrola inicializace registrů
            if (TC6Regs_GetInitDone(tc6_instance)) {
                ESP_LOGI("TC6_TASK", "TC6 registers initialization complete");
            } else {
                ESP_LOGW("TC6_TASK", "TC6 registers initialization not complete");
            }
        }

        // Periodické volání TC6Regs_CheckTimers
        TC6Regs_CheckTimers();

        // Krátké zpoždění pro úsporu CPU
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}



bool TC6_CB_OnSpiTransaction(uint8_t instance, uint8_t *pTx, uint8_t *pRx, uint16_t len, void *pGlobalTag)
{
    ESP_LOGI("TC6", "SPI Transaction: Instance %u, Length %u", instance, len);
    
    // Debug výpis pro TX buffer (max 16 bytů pro přehlednost)
 /*  ESP_LOGI("TC6", "TX data (first 16 bytes):");
    for (int i = 0; i < (len > 16 ? 16 : len); i++) {
        printf("%02X ", pTx[i]);
        if ((i + 1) % 8 == 0) printf("\n");
    }
    printf("\n");*/

    spi_transaction_t transaction = {
        .length = len * 8,
        .tx_buffer = pTx,
        .rx_buffer = pRx,
    };

    spi_device_handle_t devHandle = (spi_device_handle_t)pGlobalTag;
    esp_err_t ret = spi_device_transmit(devHandle, &transaction);
    if (ret != ESP_OK) {
        ESP_LOGE("TC6", "SPI transaction failed: %s", esp_err_to_name(ret));
        TC6_SpiBufferDone(instance, false); // DŮLEŽITÉ: Informovat TC6 o dokončení (selhání) transakce
        return false;
    }

    // Debug výpis pro RX buffer (max 16 bytů pro přehlednost)
 /*   ESP_LOGI("TC6", "RX data (first 16 bytes):");
    for (int i = 0; i < (len > 16 ? 16 : len); i++) {
        printf("%02X ", pRx[i]);
        if ((i + 1) % 8 == 0) printf("\n");
    }
    printf("\n");*/

    // DŮLEŽITÉ: Informovat TC6 o dokončení transakce
    TC6_SpiBufferDone(instance, true);

    vTaskDelay(pdMS_TO_TICKS(100));
    return true;
}

void init_tc6(void)
{
    tc6_instance = TC6_Init((void *)spi_handle);
    if (tc6_instance == NULL) {
        ESP_LOGE("TC6", "Failed to initialize TC6!");
        return;
    }

    ESP_LOGI("TC6", "TC6 initialized successfully!");
    
    // Definujte MAC adresu pro zařízení
    uint8_t macAddress[6] = {0x00, 0x04, 0xA3, 0x12, 0x34, 0x56}; // Příklad MAC adresy
    
    // Inicializace registrů TC6 - KRITICKÉ pro správnou funkčnost
    bool regs_init_ok = TC6Regs_Init(tc6_instance, NULL, macAddress, 
                                     false, // PLCA enable
                                     0,     // nodeId
                                     1,     // nodeCount
                                     0,     // burstCount
                                     0,     // burstTimer
                                     false, // promiscuous
                                     false, // txCutThrough
                                     false);// rxCutThrough
                                     
    if (!regs_init_ok) {
        ESP_LOGE("TC6", "Failed to initialize TC6 registers!");
    } else {
        ESP_LOGI("TC6", "TC6 registers initialized successfully!");
    }
    
    // Povolení přenosu dat až PO inicializaci registrů
    TC6_EnableData(tc6_instance, true);
}

static esp_err_t init_spi(void)
{
    esp_err_t ret;

    // Konfigurace SPI sběrnice
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    // Inicializace SPI sběrnice
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE("SPI", "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Konfigurace SPI zařízení
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 5 * 1000 * 1000, // 10 MHz
        .mode = 0,                          // SPI mód 0
        .spics_io_num = PIN_NUM_CS,         // Chip Select pin
        .queue_size = 7,
    };

    // Přidání zařízení na SPI sběrnici
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("SPI", "Failed to add SPI device: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI("SPI", "SPI initialized successfully");
    return ESP_OK;
}
 

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




// Callback for receiving an Ethernet packet
void TC6_CB_OnRxEthernetPacket(TC6_t *pInst, bool success, uint16_t len, uint64_t *rxTimestamp, void *pGlobalTag)
{
    if (success) {
        ESP_LOGI(TAG, "Ethernet packet received: length=%u, timestamp=%llu", len, rxTimestamp ? *rxTimestamp : 0);
    } else {
        ESP_LOGE(TAG, "Failed to receive Ethernet packet");
    }
}

// Callback for handling errors
void TC6_CB_OnError(TC6_t *pInst, TC6_Error_t err, void *pGlobalTag)
{
    ESP_LOGE(TAG, "TC6 error occurred: %d", err);
}

// Callback for receiving a slice of an Ethernet packet
void TC6_CB_OnRxEthernetSlice(TC6_t *pInst, const uint8_t *pBuf, uint16_t offset, uint16_t sliceLen, void *pGlobalTag)
{
    ESP_LOGI(TAG, "Ethernet slice received: offset=%u, length=%u", offset, sliceLen);
}

// Callback for indicating that the TC6 service needs to be called
void TC6_CB_OnNeedService(TC6_t *pInst, void *pGlobalTag)
{
    ESP_LOGI(TAG, "TC6_CB_OnNeedService called");
    // Nastavíme flag, že je potřeba obsloužit TC6
    serviceNeeded = true;
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

void handle_tc6_service(void)
{
    if (serviceNeeded) {
        serviceNeeded = false; // Resetujeme flag
        TC6_Service(tc6_instance, false);
    }
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

// Funkce pro asynchronní čtení z registru, s návratem hodnoty
static uint32_t lan8651ReadChipId(TC6_t *tc6_instance)
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











