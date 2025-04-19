#ifndef SPI_H
#define SPI_H

#include "esp_err.h"
#include "driver/spi_master.h"

// Global handle for the SPI device
extern spi_device_handle_t spi_handle;

// Iniscialization of the spi interface 
esp_err_t init_spi(void);

#endif