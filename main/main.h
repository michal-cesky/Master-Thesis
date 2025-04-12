#ifndef MAIN_H
#define MAIN_H

#include "esp_err.h"

#include "tc6.h"
#include "tc6-regs.h"

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5
#define PIN_NUM_RESET   4
#define IRQ_PIN 21

#define DEVICE 1

// Nastavení pro zařízení 1
#if DEVICE == 1
    #define DEVICE_IP      "192.168.1.10"
    #define DEVICE_NETMASK "255.255.255.0"
    #define DEVICE_GATEWAY "192.168.1.1"
    #define DEVICE_MAC     {0x00, 0x04, 0xA3, 0x34, 0x50, 0x56}

    //Client settings
    #define TARGET_IP      "192.168.1.20"
    #define TARGET_PORT    1234

// Nastavení pro zařízení 2
#elif DEVICE == 2
    #define DEVICE_IP      "192.168.1.20"
    #define DEVICE_NETMASK "255.255.255.0"
    #define DEVICE_GATEWAY "192.168.1.1"
    #define DEVICE_MAC     {0x00, 0x04, 0xA3, 0x12, 0x20, 0x56}

    #define TARGET_IP      "192.168.1.10"
    #define TARGET_PORT    1234


#endif

static const char *TAG = "TC6_MAIN";

extern uint8_t macAddress[6];
extern QueueHandle_t rxQueue;


#ifdef DEBUG
    #define DEBUG_LOG(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
#else
    // Pokud není DEBUG definován, logování se vypne
    #define DEBUG_LOG(tag, format, ...)
#endif

#endif