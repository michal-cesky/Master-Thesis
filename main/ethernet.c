#include "ethernet.h"
#include "esp_log.h"
#include "lan8651.h"
#include <string.h>
#include "esp_err.h"

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "lwip/err.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"

#include "netif/ethernet.h"
#include "esp_netif.h"
#include "main.h"

static EthernetFrame_t currentFrame;
static struct netif netif;
static uint16_t currentOffset = 0;
QueueHandle_t rxQueue = NULL;


void TC6_CB_OnRxEthernetPacket(TC6_t *pInst, bool success, uint16_t len, uint64_t *rxTimestamp, void *pGlobalTag)
{
    if (success) {
        struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
        if (p != NULL) {
            memcpy(p->payload, pGlobalTag, len);
            if (netif.input(p, &netif) != ERR_OK) {
                ESP_LOGE("ETHERNET", "Failed to pass packet to LWIP");
                pbuf_free(p);
            }
        } else {
            ESP_LOGE("ETHERNET", "Failed to allocate pbuf for received packet");
        }
    } else {
        ESP_LOGE("ETHERNET", "Failed to receive Ethernet packet");
    }
}

void TC6_CB_OnRxEthernetSlice(TC6_t *pInst, const uint8_t *pBuf, uint16_t offset, uint16_t sliceLen, void *pGlobalTag)
{

    ESP_LOGI("ETHERNET", "Ethernet slice received: offset=%u, length=%u", offset, sliceLen);

    // Pokud je offset 0, začíná nový rámec
    if (offset == 0) {
        currentOffset = 0;
        memset(&currentFrame, 0, sizeof(currentFrame)); // Vymaž aktuální rámec
    }

    // Zkontroluj, zda se rámec vejde do bufferu
    if ((currentOffset + sliceLen) <= sizeof(currentFrame.data)) {
        memcpy(currentFrame.data + currentOffset, pBuf, sliceLen); // Zkopíruj slice do bufferu
        currentOffset += sliceLen;
    } else {
        ESP_LOGE("ETHERNET", "Frame size exceeds buffer limit, dropping frame");
        currentOffset = 0; // Resetuj offset
        return;
    }

    // Pokud je slice poslední částí rámce
    if ((offset + sliceLen) >= currentFrame.length) {
        currentFrame.length = currentOffset;

        // 1) Předáme rámec LWIP, aby zpracoval ARP/IP/UDP:
        struct pbuf *p = pbuf_alloc(PBUF_RAW, currentFrame.length, PBUF_POOL);
        if (p != NULL) {
            memcpy(p->payload, currentFrame.data, currentFrame.length);
            if (netif.input(p, &netif) != ERR_OK) {
                ESP_LOGE("ETHERNET", "Failed to pass packet to LWIP");
                pbuf_free(p);
            }
        }

        ESP_LOGI("ETHERNET", "Received complete frame: length=%u", currentFrame.length);
        ESP_LOGI("ETHERNET", "Frame content: %.*s", currentFrame.length, currentFrame.data);

        // Ulož rámec do fronty pro další zpracování
        if (rxQueue != NULL) {
            if (xQueueSend(rxQueue, &currentFrame, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW("ETHERNET", "RX queue is full, dropping frame");
            }
        }
    }
}


void InitLWIP(void)
{
    ESP_LOGI("LWIP", "Initializing TCP/IP stack...");
    tcpip_init(NULL, NULL); // Inicializace TCP/IP stacku

    // Konfigurace síťového rozhraní
    ip4_addr_t ipaddr, netmask, gw;
    IP4_ADDR(&ipaddr, 192, 168, 1, 50); // Nastavení IP adresy
    IP4_ADDR(&netmask, 255, 255, 255, 0); // Nastavení masky sítě
    IP4_ADDR(&gw, 192, 168, 1, 1); // Nastavení brány

    printf("1\n");

    ESP_LOGI("LWIP", "Adding network interface...");
    if (netif_add(&netif, &ipaddr, &netmask, &gw, NULL, ethernetif_init, tcpip_input) == NULL) {
        ESP_LOGE("LWIP", "Failed to add network interface");
        return;
    }

    printf("3\n");

    netif_set_default(&netif);

    printf("4\n");

    netif_set_up(&netif);

    netif_set_link_up(&netif);

    ESP_LOGI("LWIP", "LWIP initialized successfully");
}

void SendUDPPacket(const char *data, uint16_t length, const char *dest_ip, uint16_t dest_port)
{
    struct udp_pcb *pcb = udp_new();
    if (!pcb) {
        ESP_LOGE("UDP", "Failed to create UDP PCB");
        return;
    }

    ip_addr_t dest_addr;
    if (!ipaddr_aton(dest_ip, &dest_addr)) {
        ESP_LOGE("UDP", "Invalid destination IP address");
        udp_remove(pcb);
        return;
    }

    printf("true\n");

    printf("true 2\n");


    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_RAM);
    if (!p) {
        ESP_LOGE("UDP", "Failed to allocate pbuf");
        udp_remove(pcb);
        return;
    }

    memcpy(p->payload, data, length);

    err_t err = udp_sendto(pcb, p, &dest_addr, dest_port);
    if (err == ERR_OK) {
        ESP_LOGI("UDP", "Packet sent successfully");
    } else {
        ESP_LOGE("UDP", "Failed to send packet: %d", err);
    }

    pbuf_free(p);
    udp_remove(pcb);
}

err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    printf("low_level_output called, sending frame...\n");
    struct pbuf *q;
    uint8_t buffer[1500];
    uint16_t length = 0;

    for (q = p; q != NULL; q = q->next) {
        memcpy(&buffer[length], q->payload, q->len);
        length += q->len;
    }

    bool success = TC6_SendRawEthernetPacket(tc6_instance, buffer, length, 0, NULL, NULL);
    if (!success) {
        ESP_LOGE("ETHERNET", "Failed to send Ethernet frame");
        return ERR_IF;
    }

    return ERR_OK;
}


void AppSendPacket(void *pvParameters)
{
    while (1) {
        const char *message = "Hello, here is ESP1";
        SendUDPPacket(message, strlen(message), "192.168.1.10", 1234);
        vTaskDelay(pdMS_TO_TICKS(5000)); // Odesílání každých 5 sekund
    }
}

err_t ethernetif_init(struct netif *netif)
{
    printf("ZDE\n");

    memcpy(netif->hwaddr, macAddress, sizeof(macAddress));
    netif->hwaddr_len = 6;

    netif->mtu = 1500;

    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;

    netif->output = etharp_output;
    netif->linkoutput = low_level_output;

    printf("ZDE 2\n");

    return ERR_OK;
}


void InitQueue(void) {
    rxQueue = xQueueCreate(10, sizeof(EthernetFrame_t));
    if (rxQueue == NULL) {
        ESP_LOGE("QUEUE", "Failed to create RX queue");
    }
    else {
        ESP_LOGI("QUEUE", "RX queue created successfully");
    }
}


void RxTask(void *pvParameters)
{
    EthernetFrame_t frame;

    while (1) {
        if (xQueueReceive(rxQueue, &frame, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI("RX_TASK", "Received frame: length=%u", frame.length);

            // Převod na hexadecimální řetězec
            char hex_output[frame.length * 2 + 1];
            bin_to_hex(frame.data, frame.length, hex_output, sizeof(hex_output));
            ESP_LOGI("RX_TASK", "Frame content (hex): %s", hex_output);

            // Převod na čitelný textový řetězec
            char string_output[frame.length + 1];
            bin_to_string(frame.data, frame.length, string_output, sizeof(string_output));
            ESP_LOGI("RX_TASK", "Frame content (string): %s", string_output);
        }
    }
}



void bin_to_hex(const uint8_t *data, size_t length, char *output, size_t output_size) {
    size_t i;
    for (i = 0; i < length && (i * 2 + 1) < output_size; i++) {
        sprintf(&output[i * 2], "%02X", data[i]);
    }
    output[i * 2] = '\0';
}

void bin_to_string(const uint8_t *data, size_t length, char *output, size_t output_size) {
    size_t i;
    for (i = 0; i < length && i < output_size - 1; i++) {
        if (data[i] >= 32 && data[i] <= 126) {
            output[i] = (char)data[i];
        } else {
            output[i] = '.';
        }
    }
    output[i] = '\0';
}
