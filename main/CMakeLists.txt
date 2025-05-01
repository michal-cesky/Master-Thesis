idf_component_register(SRCS "encryption.c" "main.c" "spi.c" "lan8651.c" "ethernet.c"
                       INCLUDE_DIRS "."
                       REQUIRES libtc6 driver esp_netif lwip espressif__pcap spiffs mbedtls fatfs wear_levelling)
