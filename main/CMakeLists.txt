idf_component_register(SRCS "main.c" "callback.c" "spi.c" "lan8651.c" "ethernet.c"
                       INCLUDE_DIRS "."
                       REQUIRES libtc6 driver esp_timer esp_netif lwip)
