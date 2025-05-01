#ifndef ETHERNET_H
#define ETHERNET_H

// Structure for Ethernet frame
typedef struct {
    uint8_t data[1500]; 
    uint16_t length; 
} EthernetFrame_t;

// Queue for received frames
extern QueueHandle_t rxQueue;

// Initialization functions for save received frames
void InitQueue(void);

// Initialization functions for lwIP stack
void InitLWIP(void);

// Fucntion called by task to display/save received packets
void RxTask(void *pvParameters);

// Funkcion called by task to send UDP packet
void AppSendPacket(void *pvParameters);

void ClientTask(void *pvParameters);

void ServerTask(void *pvParameters);

#endif