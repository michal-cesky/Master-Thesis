#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_netif.h"
#include <string.h>
#include <inttypes.h>

#include "main.h"
#include "spi.h"
#include "lan8651.h"
#include "ethernet.h"


uint8_t macAddress[6] = DEVICE_MAC;


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

    InitQueue();
    InitLWIP();

    xTaskCreate(SyncTask, "TC6Task", 4096, (void *)tc6_instance, 5, NULL);

    uint32_t chipId1 = lan8651ReadChipId(tc6_instance);
    ESP_LOGI("MAIN", "LAN8651 Chip ID: 0x%08lX", chipId1);
    
    uint8_t chipRev1 = TC6Regs_GetChipRevision(tc6_instance);
    ESP_LOGI("MAIN", "LAN8651 Chip Revision: %u", chipRev1);

  //  xTaskCreate(SendFrame, "SendFrame", 4096, (void *)tc6_instance, 5, NULL);

    xTaskCreate(AppSendPacket, "AppSendPacketTask", 4096, NULL, 5, NULL);
/*
    rxQueue = xQueueCreate(RX_QUEUE_SIZE, sizeof(EthernetFrame_t));
    if (rxQueue == NULL) {
    ESP_LOGE("MAIN", "Failed to create RX queue");
    return; // Ukončete aplikaci, pokud se frontu nepodaří vytvořit
    }*/


    xTaskCreate(RxTask, "RxTask", 4096, NULL, 5, NULL);
}