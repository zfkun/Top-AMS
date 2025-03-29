/* Esptouch example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once

#include "esp_eap_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdlib.h>
#include <string.h>

#include "tools.hpp"
using mstd::fpr;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
inline EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
inline const int CONNECTED_BIT = BIT0;
inline const int ESPTOUCH_DONE_BIT = BIT1;
inline const char *TAG = "smartconfig_example";


inline void smartconfig_example_task(void *parm) {
    EventBits_t uxBits;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if (uxBits & CONNECTED_BIT) {
            fpr(TAG, "WiFi Connected to ap");
        }
        if (uxBits & ESPTOUCH_DONE_BIT) {
            fpr(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}

inline void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        fpr(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        fpr(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        fpr(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = {0};
        uint8_t password[65] = {0};
        uint8_t rvd_data[33] = {0};

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

#ifdef CONFIG_SET_MAC_ADDRESS_OF_TARGET_AP
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            fpr(TAG, "Set MAC address of target AP: " MACSTR " ", MAC2STR(evt->bssid));
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }
#endif

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        fpr(TAG, "SSID:%s", ssid);
        fpr(TAG, "PASSWORD:%s", password);
        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            ESP_ERROR_CHECK(esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)));
            fpr(TAG, "RVD_DATA:");
            for (int i = 0; i < 33; i++) {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }

        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        esp_wifi_connect();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

inline void initialise_wifi(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = {.osi_funcs = &g_wifi_osi_funcs,
                              .wpa_crypto_funcs = g_wifi_default_wpa_crypto_funcs,
                              .static_rx_buf_num = CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM,
                              .dynamic_rx_buf_num = CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM,
                              .tx_buf_type = CONFIG_ESP_WIFI_TX_BUFFER_TYPE,
                              .static_tx_buf_num = WIFI_STATIC_TX_BUFFER_NUM,
                              .dynamic_tx_buf_num = WIFI_DYNAMIC_TX_BUFFER_NUM,
                              .rx_mgmt_buf_type = CONFIG_ESP_WIFI_DYNAMIC_RX_MGMT_BUF,
                              .rx_mgmt_buf_num = WIFI_RX_MGMT_BUF_NUM_DEF,
                              .cache_tx_buf_num = WIFI_CACHE_TX_BUFFER_NUM,
                              .csi_enable = WIFI_CSI_ENABLED,
                              .ampdu_rx_enable = WIFI_AMPDU_RX_ENABLED,
                              .ampdu_tx_enable = WIFI_AMPDU_TX_ENABLED,
                              .amsdu_tx_enable = WIFI_AMSDU_TX_ENABLED,
                              .nvs_enable = WIFI_NVS_ENABLED,
                              .nano_enable = WIFI_NANO_FORMAT_ENABLED,
                              .rx_ba_win = WIFI_DEFAULT_RX_BA_WIN,
                              .wifi_task_core_id = WIFI_TASK_CORE_ID,
                              .beacon_max_len = WIFI_SOFTAP_BEACON_MAX_LEN,
                              .mgmt_sbuf_num = WIFI_MGMT_SBUF_NUM,
                              .feature_caps = WIFI_FEATURE_CAPS,
                              .sta_disconnected_pm = WIFI_STA_DISCONNECTED_PM_ENABLED,
                              .espnow_max_encrypt_num = CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM,
                              .tx_hetb_queue_num = WIFI_TX_HETB_QUEUE_NUM,
                              .dump_hesigb_enable = WIFI_DUMP_HESIGB_ENABLED,
                              .magic = WIFI_INIT_CONFIG_MAGIC};
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}



// void app_main(void) {
//     ESP_ERROR_CHECK(nvs_flash_init());
//     initialise_wifi();
// }