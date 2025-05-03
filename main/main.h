#ifndef MAIN_H
#define MAIN_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "tc6-regs.h"

// Queue for received frames
extern QueueHandle_t rxQueue;

#endif