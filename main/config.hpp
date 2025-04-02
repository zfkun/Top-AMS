#pragma once

#include "espIO.hpp"
#include "esptools.hpp"
#include <array>

#if __has_include("localconfig.hpp")
#include "localconfig.hpp"
#endif

namespace config {

    using std::string;

    struct motor {
        gpio_num_t forward = GPIO_NUM_NC;
        gpio_num_t backward = GPIO_NUM_NC;

        motor() = default;
        motor(gpio_num_t f, gpio_num_t b) : forward(f), backward(b) {}
    }; // motor


    //**********************用户配置区开始******************************
    //**********************用户配置区开始******************************
    //**********************用户配置区开始******************************



    inline const auto uload_time = 5s; // 退料运转时间
    inline const auto load_time = 6s;  // 进料运转时间,时间要比退料久一些

    inline const auto forward_click = GPIO_NUM_NC; // 进料微动
    inline const auto back_click = GPIO_NUM_NC;    // 退料微动


    inline std::array<motor, 16> motors{
        // 电机要使用的GPIO
        motor{GPIO_NUM_3, GPIO_NUM_2} // 通道1,前向GPIO,后向GPIO
        ,
        motor{GPIO_NUM_5, GPIO_NUM_4} // 通道2
        ,
        motor{GPIO_NUM_NC, GPIO_NUM_NC} // 通道3
        ,
        motor{GPIO_NUM_NC, GPIO_NUM_NC} // 通道4
    }; // motors
    // 小白用户自定义电机使用GPIO时
    // 请仔细查询你开发板的针脚定义
    // 像合众esp32C3,应该避开usb使用的18,19,LED灯的12,13
    // GPIO_NUM_NC表示不使用


    inline string bambu_ip;
    inline string device_serial;
    inline string Mqtt_pass;

    //**********************用户配置区结束******************************
    //**********************用户配置区结束******************************
    //**********************用户配置区结束******************************


    inline string mqtt_server(const string &ip = bambu_ip) { return "mqtts://" + ip + ":8883"; }
    inline string mqtt_username = "bblp";
    inline string topic_subscribe(const string &_device_serial = device_serial) {
        return "device/" + _device_serial + "/report";
    }
    inline string topic_publish(const string &_device_serial = device_serial) {
        return "device/" + _device_serial + "/request";
    }





} // config