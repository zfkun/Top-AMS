#pragma once

#ifndef LOCAL_CONFIG
#define LOCAL_CONFIG
#endif

#include "esptools.hpp"


struct initconfig_t {
    initconfig_t() {
        fpr("预设配置");
        {
            mesp::ConfigStore wificonfig("wificonfig");
            wificonfig.set("Wifi_ssid", "your_wifi");
            wificonfig.set("Wifi_pass", "your_wifi_pass");
        }
        {
            mesp::ConfigStore Mqttconfig("Mqttconfig");
            Mqttconfig.set("Mqtt_ip", "192.168.1.1");
            Mqttconfig.set("Mqtt_pass", "XXXXXXXX");
            Mqttconfig.set("device_serial", "XXXXXXXXXXXXXX");
        }
    }
};
// inline initconfig_t __initconfig;
// 方便本地调试