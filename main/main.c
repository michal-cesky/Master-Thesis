#include <string.h>
#include "esp_log.h"
#include "driver/spi_master.h"

#include "main.h"
#include "spi.h"
#include "lan8651.h"
#include "ethernet.h"
#include "encryption.h"

static const char *Main_TAG = "MAIN";

uint8_t macAddress[6] = DEVICE_MAC;




void app_main(void) {
    ESP_LOGI(Main_TAG, "Initializing SPI...");
    if (InitSpi() != ESP_OK) {
        ESP_LOGE(Main_TAG, "SPI initialization failed!");
        return;
    }

    initPhyResetPin();

    ESP_LOGI(Main_TAG, "Initializing TC6...");
    initTc6();

    InitQueue();
    InitLWIP();



    if (Encrypted_server) {
        ESP_LOGI(Main_TAG, "Initializing DTLS server...");
        InitDTLSServer();
    } else {
        ESP_LOGI(Main_TAG, "Initializing DTLS client...");
        InitDTLSClient();
    }


    xTaskCreate(SyncTask, "TC6Task", 4096, (void *)tc6_instance, 5, NULL);

    uint32_t chipId1 = ReadMacControlRegister(tc6_instance);
    ESP_LOGI(Main_TAG, "LAN8651 Chip ID: 0x%08lX", chipId1);
    
    uint8_t chipRev1 = TC6Regs_GetChipRevision(tc6_instance);
    ESP_LOGI(Main_TAG, "LAN8651 Chip Revision: %u", chipRev1);

    xTaskCreate(RxTask, "RxTask", 8192, NULL, 5, NULL);
 

    ESP_LOGI(Main_TAG, "Encrypted_server is %s", Encrypted_server ? "true" : "false");

    if (Encrypted_server) {
        xTaskCreate(ServerTask, "ServerTask", 8192, NULL, 5, NULL);
        printf("Server task created\n");
    } else {
        xTaskCreate(ClientTask, "ClientTask", 8192, NULL, 5, NULL);
        printf("Client task created\n");
    }

    //xTaskCreate(AppSendPacket, "AppSendPacketTask", 4096, NULL, 5, NULL);
    
}