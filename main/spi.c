#include "esp_log.h"

#include "spi.h"
#include "main.h"

static const char *SPI_TAG = "SPI";

spi_device_handle_t spi_handle;




esp_err_t init_spi(void) {
    esp_err_t ret;

    // SPI configuraton
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    // SPI init
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // SPI device configuration
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 5 * 1000 * 1000, // 5 MHz
        .mode = 0,                         // SPI mode
        .spics_io_num = PIN_NUM_CS,        // CS pin
        .queue_size = 7,
    };

    // Add device to SPI interface
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPI initialized successfully");
    return ESP_OK;
}

// Callback from tc6 library to transfer spi data to LAN8651
bool TC6_CB_OnSpiTransaction(uint8_t instance, uint8_t *pTx, uint8_t *pRx, uint16_t len, void *pGlobalTag) {
    
    spi_transaction_t transaction = {
        .length = len * 8,
        .tx_buffer = pTx,
        .rx_buffer = pRx,
    };

    spi_device_handle_t devHandle = (spi_device_handle_t)pGlobalTag;
    esp_err_t ret = spi_device_transmit(devHandle, &transaction);
    if (ret != ESP_OK) {
        ESP_LOGE(SPI_TAG, "SPI transaction failed: %s", esp_err_to_name(ret));
        TC6_SpiBufferDone(instance, false);
        return false;
    }

    TC6_SpiBufferDone(instance, true);

    vTaskDelay(pdMS_TO_TICKS(100));
    return true;
}