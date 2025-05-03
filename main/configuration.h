#ifndef CONFIGURATION_H
#define CONFIGURATION_H


// Configuration for SPI
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5
#define PIN_NUM_RESET   4
#define IRQ_PIN 21

#define CLOCK_RATE 2 * 1000 * 1000 // 2MHz


// Configuration for different devices
#define DEVICE 3


    // Example settings for different devices
#if DEVICE == 1

    // Confguration network settings
    #define DEVICE_IP      "192.168.1.10"
    #define DEVICE_NETMASK "255.255.255.0"
    #define DEVICE_GATEWAY "192.168.1.1"


    //Configuration for client (where, witch, how the message is sent)
    #define TARGET_IP      "192.168.1.20"
    #define TARGET_PORT    "1234"
    #define MESSAGE "Hello, here is ESP32 number: 1!"
    #define TIMER_FOR_SEND_MESSAGE 10000 // Send message every 10 second


    // Configuration for LAN8651
    #define DEVICE_MAC     {0x00, 0x04, 0xA3, 0x34, 0x50, 0x56}

    // Configuration for PLCA (If PLCA is enabled, no need to setup other parameters)
    #define PLCA_ENABLE false
    #define NODE_ID 0
    #define NODE_COUNT 2
    #define BURST_COUNT 0
    #define BURST_TIMER 0
    #define TX_CUT_THROUHG false
    #define RX_CUT_THROUGH false


    // Use LAN8651 as an sniffer
    #define SNIFFER false
    #define PCAP_FILENAME "/spiffs/capture.pcap"


    // Encryption settings
    // If you dont want to use encryption, set ENCRYPTED_SERVER and ENCRYPTED_CLIENT to false
    // If you want to use encryption, set first device as an server and second device as a client
    #define ENCRYPTED_SERVER false
    #define ENCRYPTED_CLIENT false

    // Encryption settings fo server hostname
    #define SERVER_NAME "CA"

    // Encryption settings for setup pers
    #define SERVER_PERS "dtls_server"
    #define CLIENT_PERS "dtls_client"

    // Encryption settings for DTLS timer
    #define READ_TIMEOUT_MS 5000
    #define MAX_RETRY       50



#elif DEVICE == 2

    #define DEVICE_IP      "192.168.1.20"
    #define DEVICE_NETMASK "255.255.255.0"
    #define DEVICE_GATEWAY "192.168.1.1"
    
    
    #define TARGET_IP      "192.168.1.10"
    #define TARGET_PORT    "1234"
    #define MESSAGE "Hello, here is ESP32 number: 2!"
    #define TIMER_FOR_SEND_MESSAGE 10000
    

    #define DEVICE_MAC     {0x00, 0x04, 0xA3, 0x12, 0x20, 0x56}

    #define PLCA_ENABLE false
    #define NODE_ID 1
    #define NODE_COUNT 2
    #define BURST_COUNT 0
    #define BURST_TIMER 0
    #define TX_CUT_THROUHG false
    #define RX_CUT_THROUGH false


    #define SNIFFER false
    #define PCAP_FILENAME "/spiffs/capture.pcap"


    #define ENCRYPTED_SERVER false
    #define ENCRYPTED_CLIENT false

    #define SERVER_NAME "CA"

    #define SERVER_PERS "dtls_server"
    #define CLIENT_PERS "dtls_client"

    #define READ_TIMEOUT_MS 5000
    #define MAX_RETRY       50



#elif DEVICE == 3

    #define DEVICE_IP      "192.168.1.30"
    #define DEVICE_NETMASK "255.255.255.0"
    #define DEVICE_GATEWAY "192.168.1.1"


    #define TARGET_IP      "192.168.1.10"
    #define TARGET_PORT    "1234"
    #define MESSAGE "Hello, here is ESP32 number: 3!"
    #define TIMER_FOR_SEND_MESSAGE 10000


    #define DEVICE_MAC     {0x00, 0x04, 0xA3, 0x12, 0x30, 0x56}

    #define PLCA_ENABLE false
    #define NODE_ID 2
    #define NODE_COUNT 3
    #define BURST_COUNT 0
    #define BURST_TIMER 0
    #define TX_CUT_THROUHG false
    #define RX_CUT_THROUGH false


    #define SNIFFER true
    #define PCAP_FILENAME "/spiffs/capture.pcap"


    #define ENCRYPTED_SERVER false
    #define ENCRYPTED_CLIENT false

    #define SERVER_NAME "CA"

    #define SERVER_PERS "dtls_server"
    #define CLIENT_PERS "dtls_client"

    #define READ_TIMEOUT_MS 5000
    #define MAX_RETRY       50



    
#elif DEVICE == 4

    #define DEVICE_IP      "192.168.1.40"
    #define DEVICE_NETMASK "255.255.255.0"
    #define DEVICE_GATEWAY "192.168.1.1"


    #define TARGET_IP      "192.168.1.20"
    #define TARGET_PORT    "1234"
    #define MESSAGE "Hello, here is ESP32 number: 4!"
    #define TIMER_FOR_SEND_MESSAGE 10000


    #define DEVICE_MAC     {0x00, 0x04, 0xA3, 0x12, 0x40, 0x56}

    #define PLCA_ENABLE false
    #define NODE_ID 2
    #define NODE_COUNT 3
    #define BURST_COUNT 0
    #define BURST_TIMER 0
    #define TX_CUT_THROUHG false
    #define RX_CUT_THROUGH false


    #define SNIFFER false
    #define PCAP_FILENAME "/spiffs/capture.pcap"


    #define ENCRYPTED_SERVER false
    #define ENCRYPTED_CLIENT false

    #define SERVER_NAME "CA"

    #define SERVER_PERS "dtls_server"
    #define CLIENT_PERS "dtls_client"

    #define READ_TIMEOUT_MS 5000
    #define MAX_RETRY       50

#endif

#endif