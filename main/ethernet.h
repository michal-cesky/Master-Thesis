#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "tc6.h"
#include "lwip/err.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"

#include "pcap.h"

typedef struct {
    uint8_t data[1500]; // Maximální velikost Ethernetového rámce
    uint16_t length;    // Délka rámce
} EthernetFrame_t;

extern QueueHandle_t rxQueue;

void SendFrame(void *pvParameters);
void RxTask(void *pvParameters);
void TC6_CB_OnRxEthernetPacket(TC6_t *pInst, bool success, uint16_t len, uint64_t *rxTimestamp, void *pGlobalTag);
void TC6_CB_OnRxEthernetSlice(TC6_t *pInst, const uint8_t *pBuf, uint16_t offset, uint16_t sliceLen, void *pGlobalTag);

void InitLWIP(void);
void SendUDPPacket(const char *data, uint16_t length, const char *dest_ip, uint16_t dest_port);
err_t low_level_output(struct netif *netif, struct pbuf *p);
void AppSendPacket(void *pvParameters);
err_t ethernetif_init(struct netif *netif);
void InitQueue(void);
void RxTask(void *pvParameters);
void PrintARPTable(void);
void bin_to_hex(const uint8_t *data, size_t length, char *output, size_t output_size);
void bin_to_string(const uint8_t *data, size_t length, char *output, size_t output_size);
bool extract_payload(const uint8_t *frame_data, size_t frame_length, const uint8_t **payload, size_t *payload_length);

void InitSPIFFS(void);
void InitPCAP(void);
#endif