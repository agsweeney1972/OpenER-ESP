idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES opener esp_netif esp_eth driver
)

