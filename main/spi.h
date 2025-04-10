#ifndef SPI_H
#define SPI_H

#include "esp_err.h"
#include "driver/spi_master.h"

// Deklarace SPI handle
extern spi_device_handle_t spi_handle;

// Deklarace inicializační funkce
esp_err_t init_spi(void);

#endif