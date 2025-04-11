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

//#define RX_QUEUE_SIZE 10

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