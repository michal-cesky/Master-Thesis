#include "tc6.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "tc6-regs.h"

#include "esp_timer.h"

#include <stdint.h>
#include <stdbool.h>


static const char *TAG = "TC6_CALLBACKS";

static spi_device_handle_t spiHandle;



//extern bool TC6_CB_OnSpiTransaction(uint8_t tc6instance, uint8_t *pTx, uint8_t *pRx, uint16_t len, void *pGlobalTag);

bool TC6_CB_OnSpiTransaction(uint8_t instance, uint8_t *pTx, uint8_t *pRx, uint16_t len, void *pGlobalTag) {
    ESP_LOGI(TAG, "SPI Transaction: Instance %d, Length %d", instance, len);

    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = pTx,
        .rx_buffer = pRx,
    };

    // pGlobalTag je teď platným handlem
    esp_err_t ret = spi_device_transmit(spiHandle, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI transaction failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Výpis TX bufferu
    ESP_LOGI(TAG, "TX buffer:");
    for (int i = 0; i < len; i++) {
        ESP_LOGI(TAG, "  pTx[%d] = 0x%02X", i, pTx[i]);
    }

    // Výpis RX bufferu
    ESP_LOGI(TAG, "RX buffer:");
    for (int i = 0; i < len; i++) {
        ESP_LOGI(TAG, "  pRx[%d] = 0x%02X", i, pRx[i]);
    }

    return true; // Transakce proběhla úspěšně
}










void TC6_CB_OnError(TC6_t *pInst, TC6_Error_t err, void *pGlobalTag) {
    ESP_LOGE("TC6_CALLBACKS", "Error occurred: %d", err);
}


void TC6_CB_OnNeedService(TC6_t *pInst, void *pGlobalTag) {
    ESP_LOGI("TC6_CALLBACKS", "Service needed");
    TC6_Service(pInst, true);
}

void TC6Regs_CB_OnEvent(TC6_t *pInst, TC6Regs_Event_t event, void *pTag) {
    ESP_LOGI("TC6_CALLBACKS", "Event received: %d", event);
}


// Callback pro získání aktuálního času v milisekundách
uint32_t TC6Regs_CB_GetTicksMs(void) {
    return esp_log_timestamp(); // Vrátí čas v milisekundách
}

// Callback pro přijetí Ethernetového paketu
void TC6_CB_OnRxEthernetPacket(TC6_t *pInst, bool success, uint16_t len, uint64_t *rxTimestamp, void *pGlobalTag) {
    if (success) {
        if (rxTimestamp != NULL) {
            ESP_LOGI("TC6_CB", "Ethernet packet received, length: %d, timestamp: 0x%016llX", len, *rxTimestamp);
        } else {
            ESP_LOGI("TC6_CB", "Ethernet packet received, length: %d, no timestamp", len);
        }
    } else {
        ESP_LOGE("TC6_CB", "Failed to receive Ethernet packet");
    }
}

// Callback pro přijetí části Ethernetového paketu
void TC6_CB_OnRxEthernetSlice(TC6_t *pInst, const uint8_t *pBuf, uint16_t offset, uint16_t len, void *pGlobalTag) {
    ESP_LOGI("TC6_CB", "Ethernet slice received, offset: %d, length: %d", offset, len);
}


