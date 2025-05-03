#include <string.h>
#include "esp_log.h"
#include "driver/spi_master.h"

#include "configuration.h"
#include "main.h"
#include "spi.h"
#include "lan8651.h"
#include "ethernet.h"
#include "encryption.h"




void app_main(void) {

    InitSpi();

    initPhyResetPin();

    initTc6();

    InitQueue();

    InitLWIP();

    if (ENCRYPTED_SERVER) {
        InitDTLSServer();
    } else if (ENCRYPTED_CLIENT) {
        InitDTLSClient();
    }

    xTaskCreate(SyncTask, "TC6Task", 4096, (void *)tc6_instance, 5, NULL);

    ReadMacControlRegister(tc6_instance);
    
    uint8_t chipRev1 = TC6Regs_GetChipRevision(tc6_instance);
    ESP_LOGI("LAN8651", "LAN8651 Chip Revision: %u", chipRev1);

    xTaskCreate(RxTask, "RxTask", 8192, NULL, 5, NULL);

    if (ENCRYPTED_SERVER) {
        xTaskCreate(EncryptedServerTask, "EncryptedServerTask", 8192, NULL, 5, NULL);
    } else if (ENCRYPTED_CLIENT) {
        xTaskCreate(EncryptedClientTask, "EncryptedClientTask", 8192, NULL, 5, NULL);
    }

    if (!SNIFFER && !ENCRYPTED_SERVER && !ENCRYPTED_CLIENT) {
        xTaskCreate(SendPacketUnencrypted, "AppSendPacketTask", 4096, NULL, 5, NULL);
    }
}