#include "ethernet.h"
#include "esp_log.h"
#include "lan8651.h"
#include <string.h>

// Statická proměnná pro ukládání aktuálního rámce
static EthernetFrame_t currentFrame;
static uint16_t currentOffset = 0;
QueueHandle_t rxQueue = 10;

void SendFrame(void *pvParameters)
{
    uint8_t frame[64] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Cílová MAC adresa (broadcast)
        0x00, 0x04, 0xA3, 0x12, 0x34, 0x56, // Zdrojová MAC adresa
        0x08, 0x00,                         // EtherType (IPv4)
        'H', 'e', 'l', 'l', 'o', ',', ' ',
        'E', 'S', 'P', '2', '5', '5', '1', '!',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Padding
    };

    while (1) {
        bool success = TC6_SendRawEthernetPacket(tc6_instance, frame, sizeof(frame), 0, NULL, NULL);
        if (success) {
            ESP_LOGI("TX_TASK", "Frame sent successfully");
        } else {
            ESP_LOGE("TX_TASK", "Failed to send frame");
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void RxTask(void *pvParameters)
{
    EthernetFrame_t frame;

    while (1) {
        if (xQueueReceive(rxQueue, &frame, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI("RX_TASK", "Received frame: length=%u", frame.length);
            ESP_LOGI("RX_TASK", "Frame content: %.*s", frame.length, frame.data);
        }
    }
}

void TC6_CB_OnRxEthernetPacket(TC6_t *pInst, bool success, uint16_t len, uint64_t *rxTimestamp, void *pGlobalTag)
{
    if (success) {
        EthernetFrame_t frame;
        frame.length = len;
        memcpy(frame.data, pGlobalTag, len);

        if (xQueueSend(rxQueue, &frame, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW("RX_TASK", "RX queue is full, dropping frame");
        }
    } else {
        ESP_LOGE("RX_TASK", "Failed to receive Ethernet packet");
    }
}

void TC6_CB_OnRxEthernetSlice(TC6_t *pInst, const uint8_t *pBuf, uint16_t offset, uint16_t sliceLen, void *pGlobalTag)
{
    ESP_LOGI("ETHERNET", "Ethernet slice received: offset=%u, length=%u", offset, sliceLen);

    if (offset == 0) {
        currentOffset = 0;
        memset(&currentFrame, 0, sizeof(currentFrame));
    }

    if ((currentOffset + sliceLen) <= sizeof(currentFrame.data)) {
        memcpy(currentFrame.data + currentOffset, pBuf, sliceLen);
        currentOffset += sliceLen;
    } else {
        ESP_LOGE("ETHERNET", "Frame size exceeds buffer limit, dropping frame");
        currentOffset = 0;
        return;
    }

    if ((offset + sliceLen) >= currentFrame.length) {
        currentFrame.length = currentOffset;

        ESP_LOGI("ETHERNET", "Received complete frame: length=%u", currentFrame.length);
        ESP_LOGI("ETHERNET", "Frame content: %.*s", currentFrame.length, currentFrame.data);

        if (rxQueue != NULL) {
            if (xQueueSend(rxQueue, &currentFrame, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW("ETHERNET", "RX queue is full, dropping frame");
            }
        }
    }
}