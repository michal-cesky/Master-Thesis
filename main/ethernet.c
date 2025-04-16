#include <string.h>
#include "esp_log.h"


#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/err.h"
#include "lwip/etharp.h"
#include "lwip/udp.h"

#include "netif/ethernet.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "main.h"
#include "lan8651.h"
#include "ethernet.h"
#include "pcap.h"

#include "esp_spiffs.h"

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

        // Give frame to LWIP
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
    tcpip_init(NULL, NULL);

    //Netvork config
    ip4_addr_t ipaddr, netmask, gw;
    ip4addr_aton(DEVICE_IP, &ipaddr);
    ip4addr_aton(DEVICE_NETMASK, &netmask);
    ip4addr_aton(DEVICE_GATEWAY, &gw);

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
    int sock;
    struct sockaddr_in dest_addr;


    if (!ipaddr_aton(dest_ip, &dest_addr)) {
        ESP_LOGE("UDP", "Invalid destination IP address");
        return;
    }

    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE("SOCKET", "Failed to create socket");
        return;
    }
    else {
        ESP_LOGI("SOCKET", "Socket created successfully");
    }

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    if (inet_aton(dest_ip, &dest_addr.sin_addr) == 0) {
        ESP_LOGE("SOCKET", "Invalid destination IP address");
        close(sock);
        return;
    }

    // Send data
    int sent_len = sendto(sock, data, length, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (sent_len < 0) {
        ESP_LOGE("SOCKET", "Failed to send UDP packet");
    } else {
        ESP_LOGI("SOCKET", "UDP packet sent successfully: %d bytes", sent_len);
    }

    // Close socket
    if (close(sock) == 0) {
        ESP_LOGI("SOCKET", "Socket closed successfully");
    } else {
        ESP_LOGE("SOCKET", "Failed to close socket");
    }

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
        const char *message = MESSAGE;
        SendUDPPacket(message, strlen(message), TARGET_IP, TARGET_PORT);
        vTaskDelay(pdMS_TO_TICKS(10000));
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
    const uint8_t *payload = NULL;
    size_t payload_length = 0;
    pcap_file_handle_t *pcap = NULL;

    if (SNIFFER) {
        printf("SNIFFER is enabled\n");
        InitSPIFFS();

        FILE *file = fopen("/spiffs/capture.pcap", "wb");
        if (file == NULL) {
            ESP_LOGE("PCAP", "Failed to create or open PCAP file");
        }

        // Konfigurace PCAP
        pcap_config_t config = {
            .fp = file,
            .major_version = PCAP_DEFAULT_VERSION_MAJOR,
            .minor_version = PCAP_DEFAULT_VERSION_MINOR,
            .time_zone = PCAP_DEFAULT_TIME_ZONE_GMT,
            .flags = { .little_endian = 1 }
        };

        esp_err_t err = pcap_new_session(&config, &pcap);
        if (err != ESP_OK) {
            ESP_LOGE("PCAP", "Failed to create PCAP session");
            fclose(file);
        }

        // Zápis hlavičky PCAP
        if (pcap != NULL) {
            esp_err_t err = pcap_write_header(pcap, PCAP_LINK_TYPE_ETHERNET);
            if (err != ESP_OK) {
                ESP_LOGE("PCAP", "Failed to write PCAP header");
                pcap_del_session(pcap);
                fclose(file);
                printf("TOTO nesmí nastat\n");
            }
        }

    }

    while (1) {
        if (xQueueReceive(rxQueue, &frame, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI("RX_TASK", "Received frame: length=%u", frame.length);

            char hex_output[frame.length * 2 + 1];
            bin_to_hex(frame.data, frame.length, hex_output, sizeof(hex_output));
            ESP_LOGI("RX_TASK", "Frame content (hex): %s", hex_output);

            char string_output[frame.length + 1];
            bin_to_string(frame.data, frame.length, string_output, sizeof(string_output));
            ESP_LOGI("RX_TASK", "Frame content (string): %s", string_output);

            if (extract_payload(frame.data, frame.length, &payload, &payload_length)) {
                char string_output[payload_length + 1];
                bin_to_string(payload, payload_length, string_output, sizeof(string_output));
                ESP_LOGI("RX_TASK", "Payload content (string): %s", string_output);

                char payload_hex_output[payload_length * 2 + 1];
                bin_to_hex(payload, payload_length, payload_hex_output, sizeof(payload_hex_output));
                ESP_LOGI("RX_TASK", "Payload content (hex): %s", payload_hex_output);
            } else {
                ESP_LOGW("RX_TASK", "Failed to extract payload");
            }

            if (SNIFFER) {
               // WritePacketToPCAP(*pcap, &frame);

                printf("WritePacketToPCAP called\n");
                if (pcap == NULL) {
                    ESP_LOGE("PCAP", "Invalid arguments to WritePacketToPCAP");
                    return;
                }

                struct timeval tv;
                gettimeofday(&tv, NULL);

                esp_err_t err = pcap_capture_packet(pcap, frame.data, frame.length, tv.tv_sec, tv.tv_usec);
                if (err != ESP_OK) {
                    ESP_LOGE("PCAP", "Failed to write packet to PCAP file");
                } else {
                    ESP_LOGI("PCAP", "Packet written to PCAP file: length=%u", frame.length);
                }
                } else {
                    ESP_LOGI("PCAP", "PCAP is not enabled, skipping packet write");
                }


            
                    
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

bool extract_payload(const uint8_t *frame_data, size_t frame_length, const uint8_t **payload, size_t *payload_length) {
    if (frame_length < 42) {
        ESP_LOGW("PAYLOAD", "Frame too short to contain payload");
        return false;
    }

    struct ip_hdr *iphdr = (struct ip_hdr *)(frame_data + 14);
    uint16_t ip_header_length = IPH_HL(iphdr) * 4;

    if (IPH_PROTO(iphdr) != IP_PROTO_UDP) {
        ESP_LOGW("PAYLOAD", "Frame does not contain UDP payload");
        return false;
    }

    struct udp_hdr *udphdr = (struct udp_hdr *)((uint8_t *)iphdr + ip_header_length);

    uint16_t udp_length = ntohs(udphdr->len);
    uint16_t udp_header_length = sizeof(struct udp_hdr);

    if (udp_length <= udp_header_length) {
        ESP_LOGW("PAYLOAD", "UDP packet does not contain payload");
        return false;
    }

    *payload = (uint8_t *)udphdr + udp_header_length;
    *payload_length = udp_length - udp_header_length;

    if (((uint8_t *)*payload - frame_data + *payload_length) > frame_length) {
        ESP_LOGW("PAYLOAD", "Invalid payload length");
        return false;
    }

    return true;
}

void WritePacketToPCAP(pcap_file_handle_t pcap, const EthernetFrame_t *frame) {

    printf("WritePacketToPCAP called\n");
    if (pcap == NULL || frame == NULL) {
        ESP_LOGE("PCAP", "Invalid arguments to WritePacketToPCAP");
        return;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);

    esp_err_t err = pcap_capture_packet(pcap, frame->data, frame->length, tv.tv_sec, tv.tv_usec);
    if (err != ESP_OK) {
        ESP_LOGE("PCAP", "Failed to write packet to PCAP file");
    } else {
        ESP_LOGI("PCAP", "Packet written to PCAP file: length=%u", frame->length);
    }
}


void InitSPIFFS(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs", // Mount point
        .partition_label = NULL,
        .max_files = 5, // Maximální počet otevřených souborů
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE("SPIFFS", "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI("SPIFFS", "Partition size: total: %d, used: %d", total, used);
    } else {
        ESP_LOGE("SPIFFS", "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }

}

