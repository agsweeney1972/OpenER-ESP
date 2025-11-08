#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "lwip/netif.h"
#include "opener.h"

static const char *TAG = "opener_main";
static struct netif *s_netif = NULL;
static SemaphoreHandle_t s_netif_mutex = NULL;

static void ethernet_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
               mac_addr[0], mac_addr[1], mac_addr[2],
               mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    
    // Create mutex on first call if needed
    if (s_netif_mutex == NULL) {
        s_netif_mutex = xSemaphoreCreateMutex();
        if (s_netif_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create netif mutex");
            return;
        }
    }
    
    // Take mutex to protect s_netif access
    if (xSemaphoreTake(s_netif_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take netif mutex");
        return;
    }
    
    if (s_netif == NULL) {
        for (struct netif *netif = netif_list; netif != NULL; netif = netif->next) {
            if (netif_is_up(netif) && netif_is_link_up(netif)) {
                s_netif = netif;
                break;
            }
        }
    }
    
    struct netif *netif_to_use = s_netif;
    xSemaphoreGive(s_netif_mutex);
    
    if (netif_to_use != NULL) {
        opener_init(netif_to_use);
    } else {
        ESP_LOGE(TAG, "Failed to find netif");
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, 
                                               &ethernet_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, 
                                               &got_ip_event_handler, eth_netif));

    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    
    phy_config.phy_addr = 0;
    phy_config.reset_gpio_num = -1;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));

    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, glue));
    
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
}

