﻿#pragma once

#include "espIO.hpp"
#include "esptools.hpp"
// #include <WString.h>
#include <array>


#include "web_sync.hpp"
#define MOTORS_6// 使用当前配置（6通道）注释掉就是8通道
#define LOCAL_CONFIG

namespace config {

    // using string = String;
    using std::string;

    struct motor {
        gpio_num_t forward = GPIO_NUM_NC;
        gpio_num_t backward = GPIO_NUM_NC;

        mesp::wsStoreValue<string> name;//耗材名
        mesp::wsStoreValue<string> color;//颜色
        mesp::wsStoreValue<int> next_channel;//续料通道

        motor() = default;
        motor(int i, gpio_num_t f, gpio_num_t b)
            : forward(f),
              backward(b),
              name("ext" + std::to_string(i) + "_name", "PETG"),
              color("ext" + std::to_string(i) + "_color", "#ffffff"),
              next_channel("ext" + std::to_string(i) + "_next", i) {}
    };// motor


    //**********************用户配置区开始******************************
    //**********************用户配置区开始******************************
    //**********************用户配置区开始******************************


    inline mesp::wsStoreValue<int> load_time("load_time", 6000);// 进料运转时间
    inline mesp::wsStoreValue<int> uload_time("uload_time", 5000);// 退料运转时间

    inline mesp::wsStoreValue<int> load_time_1("load_time_1", 0);// 通道1 进料运转时间
    inline mesp::wsStoreValue<int> uload_time_1("uload_time_1", 0);// 通道1 退料运转时间

    inline mesp::wsStoreValue<int> load_time_2("load_time_2", 0);// 通道2 进料运转时间
    inline mesp::wsStoreValue<int> uload_time_2("uload_time_2", 0);// 通道2 退料运转时间

    inline mesp::wsStoreValue<int> load_time_3("load_time_3", 0);// 通道3 进料运转时间
    inline mesp::wsStoreValue<int> uload_time_3("uload_time_3", 0);// 通道3 退料运转时间

    inline mesp::wsStoreValue<int> load_time_4("load_time_4", 0);// 通道4 进料运转时间
    inline mesp::wsStoreValue<int> uload_time_4("uload_time_4", 0);// 通道4 退料运转时间

    inline mesp::wsStoreValue<int> load_time_5("load_time_5", 0);// 通道5 进料运转时间
    inline mesp::wsStoreValue<int> uload_time_5("uload_time_5", 0);// 通道5 退料运转时间

    inline mesp::wsStoreValue<int> load_time_6("load_time_6", 0);// 通道6 进料运转时间
    inline mesp::wsStoreValue<int> uload_time_6("uload_time_6", 0);// 通道6 退料运转时间

    inline mesp::wsStoreValue<int> load_time_7("load_time_7", 0);// 通道7 进料运转时间
    inline mesp::wsStoreValue<int> uload_time_7("uload_time_7", 0);// 通道7 退料运转时间

    inline mesp::wsStoreValue<int> load_time_8("load_time_8", 0);// 通道8 进料运转时间
    inline mesp::wsStoreValue<int> uload_time_8("uload_time_8", 0);// 通道8 退料运转时间

    // 通道进料运转时间查询表
    inline const std::array<mesp::wsStoreValue<int>*, 8> load_time_list = {
        &load_time_1,
        &load_time_2,
        &load_time_3,
        &load_time_4,
        &load_time_5,
        &load_time_6,
        &load_time_7,
        &load_time_8
    };

    // 通道退料运转时间查询表
    inline const std::array<mesp::wsStoreValue<int>*, 8> uload_time_list = {
        &uload_time_1,
        &uload_time_2,
        &uload_time_3,
        &uload_time_4,
        &uload_time_5,
        &uload_time_6,
        &uload_time_7,
        &uload_time_8
    };

#ifdef MOTORS_6
    inline gpio_num_t forward_click = GPIO_NUM_4;//进料微动
#else
    inline gpio_num_t forward_click = GPIO_NUM_7;
#endif
    // inline const auto back_click = GPIO_NUM_NC;   // 退料微动

    inline gpio_num_t LED_R = GPIO_NUM_12;
    inline gpio_num_t LED_L = GPIO_NUM_13;//暂未使用





inline std::array<motor, 8> motors{
#ifdef MOTORS_6
    // 当前配置（6通道）
    motor{1, GPIO_NUM_1,  GPIO_NUM_0},   // 通道1
    motor{2, GPIO_NUM_19, GPIO_NUM_18},  // 通道2
    motor{3, GPIO_NUM_3,  GPIO_NUM_2},   // 通道3
    motor{4, GPIO_NUM_6,  GPIO_NUM_10},  // 通道4
    motor{5, GPIO_NUM_12, GPIO_NUM_7},   // 通道5
    motor{6, GPIO_NUM_8,  GPIO_NUM_5},   // 通道6
    motor{7, GPIO_NUM_NC, GPIO_NUM_NC},  // 通道7（未使用）
    motor{8, GPIO_NUM_NC, GPIO_NUM_NC}   // 通道8（未使用）
#else
    // 备用配置（8通道）
    motor{1, GPIO_NUM_2,  GPIO_NUM_3},   // 通道1
    motor{2, GPIO_NUM_10, GPIO_NUM_6},   // 通道2
    motor{3, GPIO_NUM_5,  GPIO_NUM_4},   // 通道3
    motor{4, GPIO_NUM_8,  GPIO_NUM_9},   // 通道4
    motor{5, GPIO_NUM_0,  GPIO_NUM_1},   // 通道5
    motor{6, GPIO_NUM_20, GPIO_NUM_21},  // 通道6
    motor{7, GPIO_NUM_12, GPIO_NUM_13},  // 通道7（注意：GPIO12、13可能和LED冲突）
    motor{8, GPIO_NUM_18, GPIO_NUM_19}   // 通道8（注意：GPIO18、19和USB冲突）
#endif
};

// 使用说明：
// 1. 小白用户自定义电机GPIO时，请仔细查询开发板的引脚定义
// 2. 例如合宙ESP32-C3应该避开：
//    - USB使用的18、19
//    - LED灯的12、13
// 3. GPIO_NUM_NC表示该引脚不使用



    mesp::wsValue<bool> MQTT_done("MQTT_done", false);
    mesp::wsStoreValue<string> bambu_ip("bambu_ip", "192.168.1.1");
    mesp::wsStoreValue<string> MQTT_pass("MQTT_pass", "");
    mesp::wsStoreValue<string> device_serial("device_serial", "");

    //**********************用户配置区结束******************************
    //**********************用户配置区结束******************************
    //**********************用户配置区结束******************************


    inline string mqtt_server(const string& ip = bambu_ip) { return "mqtts://" + ip + ":8883"; }
    inline string mqtt_username = "bblp";
    inline string topic_subscribe(const string& _device_serial = device_serial) {
        return "device/" + _device_serial + "/report";
    }
    inline string topic_publish(const string& _device_serial = device_serial) {
        return "device/" + _device_serial + "/request";
    }





}// config