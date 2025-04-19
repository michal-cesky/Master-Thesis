#include <string.h>
#include "esp_log.h"
#include "driver/spi_master.h"

#include "main.h"
#include "spi.h"
#include "lan8651.h"
#include "ethernet.h"

static const char *Main_TAG = "MAIN";

uint8_t macAddress[6] = DEVICE_MAC;




void app_main(void) {
    ESP_LOGI(Main_TAG, "Initializing SPI...");
    if (init_spi() != ESP_OK) {
        ESP_LOGE(Main_TAG, "SPI initialization failed!");
        return;
    }

    initPhyResetPin();

    ESP_LOGI(Main_TAG, "Initializing TC6...");
    initTc6();

    InitQueue();
    InitLWIP();

    xTaskCreate(SyncTask, "TC6Task", 4096, (void *)tc6_instance, 5, NULL);

    uint32_t chipId1 = lan8651ReadChipId(tc6_instance);
    ESP_LOGI(Main_TAG, "LAN8651 Chip ID: 0x%08lX", chipId1);
    
    uint8_t chipRev1 = TC6Regs_GetChipRevision(tc6_instance);
    ESP_LOGI(Main_TAG, "LAN8651 Chip Revision: %u", chipRev1);

    xTaskCreate(RxTask, "RxTask", 4096, NULL, 5, NULL);
    
    xTaskCreate(AppSendPacket, "AppSendPacketTask", 4096, NULL, 5, NULL);
}