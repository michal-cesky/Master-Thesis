#ifndef MAIN_H
#define MAIN_H

#include "tc6-regs.h"

static const char *TAG = "MAIN";

// Global variable for the MAC address
extern uint8_t macAddress[6];

// Queue for received frames
extern QueueHandle_t rxQueue;

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5
#define PIN_NUM_RESET   4
#define IRQ_PIN 21

#define DEVICE 2


// Configurations for different devices
#if DEVICE == 1
    #define DEVICE_IP      "192.168.1.10"
    #define DEVICE_NETMASK "255.255.255.0"
    #define DEVICE_GATEWAY "192.168.1.1"
    #define DEVICE_MAC     {0x00, 0x04, 0xA3, 0x34, 0x50, 0x56}

    //Client settings
    #define TARGET_IP      "192.168.1.20"
    #define TARGET_PORT    1234

    #define MESSAGE "Hello, here is ESP32 number1?!"

    #define PLCA_ENABLE true
    #define NODE_ID 0
    #define NODE_COUNT 3

    #define SNIFFER false



#elif DEVICE == 2
    #define DEVICE_IP      "192.168.1.20"
    #define DEVICE_NETMASK "255.255.255.0"
    #define DEVICE_GATEWAY "192.168.1.1"
    #define DEVICE_MAC     {0x00, 0x04, 0xA3, 0x12, 0x20, 0x56}

    #define TARGET_IP      "192.168.1.10"
    #define TARGET_PORT    1234

    #define MESSAGE "Hello, here is ESP32 number2?!"

    #define PLCA_ENABLE true
    #define NODE_ID 1
    #define NODE_COUNT 3

    #define SNIFFER true



#elif DEVICE == 3
    #define DEVICE_IP      "192.168.1.30"
    #define DEVICE_NETMASK "255.255.255.0"
    #define DEVICE_GATEWAY "192.168.1.1"
    #define DEVICE_MAC     {0x00, 0x04, 0xA3, 0x12, 0x30, 0x56}

    #define TARGET_IP      "192.168.1.10"
    #define TARGET_PORT    1234

    #define MESSAGE "Hello, here is ESP32 number3?!"

    #define PLCA_ENABLE true
    #define NODE_ID 2
    #define NODE_COUNT 3

    #define SNIFFER false

#endif

#endif