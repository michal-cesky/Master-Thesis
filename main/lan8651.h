#ifndef LAN8651_H
#define LAN8651_H

#include "driver/spi_master.h"
#include "tc6.h"

// Global pointer to the tc6 instance
extern TC6_t *tc6_instance;

// Function for hardware reset of the PHY
void initPhyResetPin(void);

// Iniscialization of the tc6 library
void initTc6(void);

// Function to chack synchronization status of the PHY
void SyncTask(void *pvParameters);

// Function for chack MAC Network Control Register (Is transmit and receive enabled?)
uint32_t ReadMacControlRegister(TC6_t *tc6_instance);

#endif