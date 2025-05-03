#ifndef LAN8651_H
#define LAN8651_H

#include "driver/spi_master.h"
#include "tc6.h"

// Global pointer to the tc6 instance
extern TC6_t *tc6_instance;

// Global variable for the MAC address
extern uint8_t macAddress[6];

// Function for hardware reset of the PHY
void initPhyResetPin(void);

// Iniscialization of the tc6 library
void initTc6(void);

// Function to chack synchronization status of the PHY
void SyncTask(void *pvParameters);

// Function for chack MAC Network Control Register (Is transmit and receive enabled?)
void ReadMacControlRegister(TC6_t *tc6_instance);

#endif