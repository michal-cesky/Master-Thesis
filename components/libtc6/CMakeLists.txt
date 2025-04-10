idf_component_register(SRCS "src/tc6.c" "src/tc6-regs.c" # Přidejte všechny zdrojové soubory
                       INCLUDE_DIRS "inc"
                       REQUIRES driver esp_timer)  