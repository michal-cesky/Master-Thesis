#include <sys/socket.h>
#include "esp_log.h"

#include "lwip/tcpip.h"
#include "lwip/etharp.h"
#include "lwip/udp.h"

#include "esp_spiffs.h"

#include "main.h"
#include "lan8651.h"
#include "ethernet.h"
#include "pcap.h"

static const char *Ethernet_TAG = "ETHERNET";
static const char *LWIP_TAG = "LWIP";
static const char *Queue_TAG = "LWIP";
static const char *Socket_TAG = "SOCKET";
static const char *Receive_TAG = "RECEIVE";
static const char *PCAP_TAG = "PCAP";
static const char *Payload_TAG = "PAYLOAD";
static const char *Spiffs_TAG = "SPIFFS";

static EthernetFrame_t currentFrame;
QueueHandle_t rxQueue = NULL;
static uint16_t currentOffset = 0;

// Network interface registered with lwIP
static struct netif netif;

// Handle for the PCAP file
pcap_file_handle_t *pcap = NULL;

// Function for crate, close and sanding packets using UDP socket
void SendUDPPacket(const char *data, uint16_t length, const char *dest_ip, uint16_t dest_port);

// Initialization function for setap network interface in lwIP stack
err_t ethernetif_init(struct netif *netif);

// Function for convert binary recived frames to hex string
void bin_to_hex(const uint8_t *data, size_t length, char *output, size_t output_size);

// Function for convert binary recived frames to string
void bin_to_string(const uint8_t *data, size_t length, char *output, size_t output_size);

// Function for extract UDP payload from recived frame
bool extract_payload(const uint8_t *frame_data, size_t frame_length, const uint8_t **payload, size_t *payload_length);

// Function for initialize SPIFFS filesystem
void InitSPIFFS(void);

// Function for initialize PCAP file for saving recived frames
void InitPCAP(void);




// Callback function for receiving Ethernet packets
void TC6_CB_OnRxEthernetPacket(TC6_t *pInst, bool success, uint16_t len, uint64_t *rxTimestamp, void *pGlobalTag) {
    if (success) {
        struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
        if (p != NULL) {
            memcpy(p->payload, pGlobalTag, len);
            if (netif.input(p, &netif) != ERR_OK) {
                ESP_LOGE(Ethernet_TAG, "Failed to pass packet to LWIP");
                pbuf_free(p);
            }
        } else {
            ESP_LOGE(Ethernet_TAG, "Failed to allocate pbuf for received packet");
        }
    } else {
        ESP_LOGE(Ethernet_TAG, "Failed to receive Ethernet packet");
    }
}

// Callback function for receiving Ethernet slices
void TC6_CB_OnRxEthernetSlice(TC6_t *pInst, const uint8_t *pBuf, uint16_t offset, uint16_t sliceLen, void *pGlobalTag) {
    ESP_LOGI(Ethernet_TAG, "Ethernet slice received: offset=%u, length=%u", offset, sliceLen);

    if (offset == 0) {
        currentOffset = 0;
        memset(&currentFrame, 0, sizeof(currentFrame)); 
    }

    if ((currentOffset + sliceLen) <= sizeof(currentFrame.data)) {
        memcpy(currentFrame.data + currentOffset, pBuf, sliceLen);
        currentOffset += sliceLen;
    } else {
        ESP_LOGE(Ethernet_TAG, "Frame size exceeds buffer limit, dropping frame");
        currentOffset = 0;
        return;
    }

    if ((offset + sliceLen) >= currentFrame.length) {
        currentFrame.length = currentOffset;

        struct pbuf *p = pbuf_alloc(PBUF_RAW, currentFrame.length, PBUF_POOL);
        if (p != NULL) {
            memcpy(p->payload, currentFrame.data, currentFrame.length);
            if (netif.input(p, &netif) != ERR_OK) {
                ESP_LOGE(Ethernet_TAG, "Failed to pass packet to LWIP");
                pbuf_free(p);
            }
        }

        ESP_LOGI(Ethernet_TAG, "Received complete frame: length=%u", currentFrame.length);
        ESP_LOGI(Ethernet_TAG, "Frame content: %.*s", currentFrame.length, currentFrame.data);

        if (rxQueue != NULL) {
            if (xQueueSend(rxQueue, &currentFrame, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(Ethernet_TAG, "RX queue is full, dropping frame");
            }
        }
    }
}



void InitQueue(void) {
    rxQueue = xQueueCreate(50, sizeof(EthernetFrame_t));
    if (rxQueue == NULL) {
        ESP_LOGE(Queue_TAG, "Failed to create RX queue");
    }
    else {
        ESP_LOGI(Queue_TAG, "RX queue created successfully");
    }
}

void InitLWIP(void) {
    ESP_LOGI(LWIP_TAG, "Initializing TCP/IP stack...");
    tcpip_init(NULL, NULL);

    //Netvork config
    ip4_addr_t ipaddr, netmask, gw;
    ip4addr_aton(DEVICE_IP, &ipaddr);
    ip4addr_aton(DEVICE_NETMASK, &netmask);
    ip4addr_aton(DEVICE_GATEWAY, &gw);

    ESP_LOGI(LWIP_TAG, "Adding network interface...");
    if (netif_add(&netif, &ipaddr, &netmask, &gw, NULL, ethernetif_init, tcpip_input) == NULL) {
        ESP_LOGE(LWIP_TAG, "Failed to add network interface");
        return;
    }

    netif_set_default(&netif);
    netif_set_up(&netif);
    netif_set_link_up(&netif);

    ESP_LOGI(LWIP_TAG, "LWIP initialized successfully");
}



void AppSendPacket(void *pvParameters) {
    while (1) {
        const char *message = MESSAGE;
        SendUDPPacket(message, strlen(message), TARGET_IP, TARGET_PORT);
        vTaskDelay(pdMS_TO_TICKS(10000));   //Send packet every 10 seconds
    }
}

void SendUDPPacket(const char *data, uint16_t length, const char *dest_ip, uint16_t dest_port) {
    int sock;
    struct sockaddr_in dest_addr;

    if (!ipaddr_aton(dest_ip, &dest_addr)) {
        ESP_LOGE("UDP", "Invalid destination IP address");
        return;
    }

    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(Socket_TAG, "Failed to create socket");
        return;
    }
    else {
        ESP_LOGI(Socket_TAG, "Socket created successfully");
    }

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    if (inet_aton(dest_ip, &dest_addr.sin_addr) == 0) {
        ESP_LOGE(Socket_TAG, "Invalid destination IP address");
        close(sock);
        return;
    }

    // Send data
    int sent_len = sendto(sock, data, length, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (sent_len < 0) {
        ESP_LOGE(Socket_TAG, "Failed to send UDP packet");
    } else {
        ESP_LOGI(Socket_TAG, "UDP packet sent successfully: %d bytes", sent_len);
    }

    // Close socket
    if (close(sock) == 0) {
        ESP_LOGI(Socket_TAG, "Socket closed successfully");
    } else {
        ESP_LOGE(Socket_TAG, "Failed to close socket");
    }
}

// Callback function for sending Ethernet frames
err_t low_level_output(struct netif *netif, struct pbuf *p) {
    struct pbuf *q;
    uint8_t buffer[1500];
    uint16_t length = 0;

    for (q = p; q != NULL; q = q->next) {
        memcpy(&buffer[length], q->payload, q->len);
        length += q->len;
    }

    bool success = TC6_SendRawEthernetPacket(tc6_instance, buffer, length, 0, NULL, NULL);
    if (!success) {
        ESP_LOGE(Ethernet_TAG, "Failed to send Ethernet frame");
        return ERR_IF;
    }

    return ERR_OK;
}

err_t ethernetif_init(struct netif *netif) {
    memcpy(netif->hwaddr, macAddress, sizeof(macAddress));
    netif->hwaddr_len = 6;

    netif->mtu = 1500;

    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;

    netif->output = etharp_output;
    netif->linkoutput = low_level_output;

    return ERR_OK;
}



void RxTask(void *pvParameters) {
    EthernetFrame_t frame;
    const uint8_t *payload = NULL;
    size_t payload_length = 0;
    struct timeval tv;

    if (SNIFFER) {
        InitPCAP();
    }

    while (1) {
        if (xQueueReceive(rxQueue, &frame, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(Receive_TAG, "Received frame: length=%u", frame.length);

            char hex_output[frame.length * 2 + 1];
            bin_to_hex(frame.data, frame.length, hex_output, sizeof(hex_output));
            ESP_LOGI(Receive_TAG, "Frame content (hex): %s", hex_output);

            char string_output[frame.length + 1];
            bin_to_string(frame.data, frame.length, string_output, sizeof(string_output));
            ESP_LOGI(Receive_TAG, "Frame content (string): %s", string_output);

            if (extract_payload(frame.data, frame.length, &payload, &payload_length)) {
                char string_output[payload_length + 1];
                bin_to_string(payload, payload_length, string_output, sizeof(string_output));
                ESP_LOGI(Receive_TAG, "Payload content (string): %s", string_output);

                char payload_hex_output[payload_length * 2 + 1];
                bin_to_hex(payload, payload_length, payload_hex_output, sizeof(payload_hex_output));
                ESP_LOGI(Receive_TAG, "Payload content (hex): %s", payload_hex_output);
            } else {
                ESP_LOGW(Receive_TAG, "Failed to extract payload");
            }

            if (SNIFFER) {
                if (pcap == NULL) {
                    ESP_LOGE(PCAP_TAG, "Invalid arguments to WritePacketToPCAP");
                    return;
                }

                gettimeofday(&tv, NULL);

                esp_err_t err = pcap_capture_packet(pcap, frame.data, frame.length, tv.tv_sec, tv.tv_usec);
                if (err != ESP_OK) {
                    ESP_LOGE(PCAP_TAG, "Failed to write packet to PCAP file");
                } else {
                    ESP_LOGI(PCAP_TAG, "Packet written to PCAP file: length=%u", frame.length);
                }                    
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
        ESP_LOGW(Payload_TAG, "Frame too short to contain payload");
        return false;
    }

    struct ip_hdr *iphdr = (struct ip_hdr *)(frame_data + 14);
    uint16_t ip_header_length = IPH_HL(iphdr) * 4;

    if (IPH_PROTO(iphdr) != IP_PROTO_UDP) {
        ESP_LOGW(Payload_TAG, "Frame does not contain UDP payload");
        return false;
    }

    struct udp_hdr *udphdr = (struct udp_hdr *)((uint8_t *)iphdr + ip_header_length);

    uint16_t udp_length = ntohs(udphdr->len);
    uint16_t udp_header_length = sizeof(struct udp_hdr);

    if (udp_length <= udp_header_length) {
        ESP_LOGW(Payload_TAG, "UDP packet does not contain payload");
        return false;
    }

    *payload = (uint8_t *)udphdr + udp_header_length;
    *payload_length = udp_length - udp_header_length;

    if (((uint8_t *)*payload - frame_data + *payload_length) > frame_length) {
        ESP_LOGW(Payload_TAG, "Invalid payload length");
        return false;
    }

    return true;
}



void InitSPIFFS(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(Spiffs_TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(Spiffs_TAG, "Partition size: total: %d, used: %d", total, used);
    } else {
        ESP_LOGE(Spiffs_TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }

}

void InitPCAP(void) {
    ESP_LOGI(PCAP_TAG, "Sniffer mode is enabled, writing packet to PCAP file");
    InitSPIFFS();

    FILE *file = fopen("/spiffs/capture.pcap", "wb");
    if (file == NULL) {
        ESP_LOGE(PCAP_TAG, "Failed to create or open PCAP file");
    }

    pcap_config_t config = {
        .fp = file,
        .major_version = PCAP_DEFAULT_VERSION_MAJOR,
        .minor_version = PCAP_DEFAULT_VERSION_MINOR,
        .time_zone = PCAP_DEFAULT_TIME_ZONE_GMT,
        .flags = { .little_endian = 0 }
    };

    esp_err_t err = pcap_new_session(&config, &pcap);
    if (err != ESP_OK) {
        ESP_LOGE(PCAP_TAG, "Failed to create PCAP session");
        fclose(file);
    }

    // Write PCAP header
    if (pcap != NULL) {
        esp_err_t err = pcap_write_header(pcap, PCAP_LINK_TYPE_ETHERNET);
        if (err != ESP_OK) {
            ESP_LOGE(PCAP_TAG, "Failed to write PCAP header");
            pcap_del_session(pcap);
            fclose(file);
        }
    }
}