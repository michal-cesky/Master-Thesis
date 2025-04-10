#ifndef LAN8651_H
#define LAN8651_H


#include "esp_err.h"
#include "driver/spi_master.h"
#include "tc6.h"

extern TC6_t *tc6_instance;


void initPhyResetPin(void);
void init_tc6(void);
void SyncTask(void *pvParameters);
uint32_t lan8651ReadChipId(TC6_t *tc6_instance);
static void printChipRevision(TC6_t *pInst);
static void onReadChipId(TC6_t *pInst, bool success, uint32_t addr, uint32_t regValue, void *pTag, void *pGlobalTag);
static void handle_tc6_service(void);

#endif