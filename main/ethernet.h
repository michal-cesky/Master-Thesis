#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "tc6.h"

typedef struct {
    uint8_t data[1500]; // Maximální velikost Ethernetového rámce
    uint16_t length;    // Délka rámce
} EthernetFrame_t;

extern QueueHandle_t rxQueue;

void SendFrame(void *pvParameters);
void RxTask(void *pvParameters);
void TC6_CB_OnRxEthernetPacket(TC6_t *pInst, bool success, uint16_t len, uint64_t *rxTimestamp, void *pGlobalTag);
void TC6_CB_OnRxEthernetSlice(TC6_t *pInst, const uint8_t *pBuf, uint16_t offset, uint16_t sliceLen, void *pGlobalTag);

#endif