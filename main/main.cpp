#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>


// #include <esp_mq

#include "esptools.hpp"

#if __has_include("localconfig.hpp")
#include "localconfig.hpp"
#endif

#include "bambu.hpp"
#include "espIO.hpp"
#include "espMQTT.hpp"

#include "channel.hpp"

#include "esp_timer.h"

#include "web_sync.hpp"

using std::string;


int bed_target_temper_max = 0;
mesp::wsValue<std::string> ws_extruder("extruder", "1");//@_@先不存盘,也不给前端修改
std::atomic<int> extruder = 1;// 1-16,初始通道默认为1
int sequence_id = -1;
// std::atomic<int> print_error = 0;
std::atomic<int> ams_status = -1;
std::atomic<bool> pause_lock{false};// 暂停锁


inline constexpr int 正常 = 0;
inline constexpr int 退料完成需要退线 = 260;
inline constexpr int 退料完成 = 0;// 同正常
inline constexpr int 进料检查 = 262;
inline constexpr int 进料冲刷 = 263;// 推测
inline constexpr int 进料完成 = 768;
//@_@应该放在一个枚举类里

AsyncWebServer server(80);
// AsyncWebSocket ws("/ws");
AsyncWebSocket& ws = mesp::ws_server;//先直接用全局的ws_server

inline mstd::channel_lock<std::function<void()>> async_channel;//异步任务通道


// @brief 控制电机运行(前向或后向)
// @param moter_id 电机编号,从 1 开始
// @param fwd 标识方向，true 表示前向，false 表示后向
inline void motor_run(int moter_id, bool fwd) {
    moter_id--;
    if (config::motors[moter_id].forward == config::LED_R) [[unlikely]] {
        config::LED_R = GPIO_NUM_NC;
        config::LED_L = GPIO_NUM_NC;
    }//使用到了通道7,关闭代码中的LED控制
    if (fwd) {
        esp::gpio_out(config::motors[moter_id].forward, true);
        mstd::delay(config::load_time.get_value());
        esp::gpio_out(config::motors[moter_id].forward, false);
    } else {
        esp::gpio_out(config::motors[moter_id].backward, true);
        mstd::delay(config::uload_time.get_value());
        esp::gpio_out(config::motors[moter_id].backward, false);
    }
}//motor_run

template <typename T>
inline void motor_run(int moter_id, bool fwd, T&& t) {
    moter_id--;
    if (config::motors[moter_id].forward == config::LED_R) [[unlikely]] {
        config::LED_R = GPIO_NUM_NC;
        config::LED_L = GPIO_NUM_NC;
    }//使用到了通道7,关闭代码中的LED控制
    if (fwd) {
        esp::gpio_out(config::motors[moter_id].forward, true);
        mstd::delay(std::forward<T>(t));// 使用传入的延时
        esp::gpio_out(config::motors[moter_id].forward, false);
    } else {
        esp::gpio_out(config::motors[moter_id].backward, true);
        mstd::delay(std::forward<T>(t));// 使用传入的延时
        esp::gpio_out(config::motors[moter_id].backward, false);
    }
}//motor_run



//@brief WebSocket消息打印
inline void webfpr(AsyncWebSocket& ws, const string& str) {
    mstd::fpr("wsmsg: ", str);
    JsonDocument doc;
    doc["log"] = str;
    String msg;
    serializeJson(doc, msg);
    ws.textAll(msg);
}


//@brief 发布消息到MQTT服务器
void publish(esp_mqtt_client_handle_t client, const std::string& msg) {
    esp::gpio_out(config::LED_L, true);
    // mstd::delay(2s);
    fpr("发送消息:", msg);
    int msg_id = esp_mqtt_client_publish(client, config::topic_publish().c_str(), msg.c_str(), msg.size(), 0, 0);
    //@_@这里的publish用到了topic_publish(默认),耦合了
    if (msg_id < 0)
        fpr("发送失败");
    else
        fpr("发送成功,消息id=", msg_id);
    // fpr(TAG, "binary sent with msg_id=%d", msg_id);
    esp::gpio_out(config::LED_L, false);
    mstd::delay(2s);//@_@这些延时还可以调整看看
}

//@brief MQTT回调函数
void callback_fun(esp_mqtt_client_handle_t client, const std::string& json) {// 接受到信息的回调
    // fpr(json);
    using namespace ArduinoJson;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);


    static int bed_target_temper = -1;
    static int nozzle_target_temper = -1;
    bed_target_temper = doc["print"]["bed_target_temper"] | bed_target_temper;
    // nozzle_target_temper = doc["print"]["nozzle_target_temper"] | nozzle_target_temper;
    std::string gcode_state = doc["print"]["gcode_state"] | "unkonw";

    // fpr("nozzle_target_temper:",nozzle_target_temper);

    // return;
    if (bed_target_temper > 0 && bed_target_temper < 17) {// 读到的温度是通道
        if (gcode_state == "PAUSE") {
            // mstd::delay(4s);//确保暂停动作(3.5s)完成
            mstd::delay(4500ms);// 貌似4s还是有可能会有bug
            if (bed_target_temper_max > 0) {// 似乎热床置零会导致热端固定到90
                publish(client, bambu::msg::runGcode(
                                    std::string("M190 S") + std::to_string(bed_target_temper_max)// 恢复原来的热床温度
                                    // + std::string(R"(\nM109 S255)")//提前升温,9系命令自带阻塞,应该无法使两条一起生效
                                    ));
            }

            if (extruder.exchange(bed_target_temper) != bed_target_temper) {
                fpr("唤醒换料程序");
                pause_lock = true;
                extruder.notify_one();// 唤醒耗材切换
            } else if (!pause_lock.load()) {// 可能会收到旧消息
                fpr("同一耗材,无需换料");
                publish(client, bambu::msg::print_resume);// 无须换料
            }
            if (bed_target_temper_max > 0)
                bed_target_temper = bed_target_temper_max;// 必要,恢复温度后,MQTT的更新可能不及时

        } else {
            // publish(client,bambu::msg::get_status);//从第二次暂停开始,PAUSE就不会出现在常态消息里,不知道怎么回事
            // 还是会的,只是不一定和温度改变在一条json里
        }
    } else if (bed_target_temper == 0)
        bed_target_temper_max = 0;// 打印结束or冷打印版
    else
        bed_target_temper_max = std::max(bed_target_temper, bed_target_temper_max);// 不同材料可能底板温度不一样,这里选择维持最高的

    // int print_error_now = doc["print"]["print_error"] | -1;
    // if (print_error_now != -1) {
    //	fpr_value(print_error_now);
    //	if (print_error.exchange(print_error_now) != print_error_now)//@_@这种有变动才唤醒的地方可以合并一下
    //		print_error.notify_one();
    // }

    int ams_status_now = doc["print"]["ams_status"] | -1;
    if (ams_status_now != -1) {
        fpr("asm_status_now:", ams_status_now);
        if (ams_status.exchange(ams_status_now) != ams_status_now)
            ams_status.notify_one();
    }

}// callback



void work(mesp::Mqttclient client) {// 需要更好名字

    auto fpr = [](const string& r) { webfpr(ws, r); };//重设一下fpr

    client.subscribe(config::topic_subscribe());// 订阅消息

    int old_extruder = extruder;
    while (true) {
        esp::gpio_out(config::LED_R, true);
        fpr("等待换料");
        extruder.wait(old_extruder);
        esp::gpio_out(config::LED_R, false);

        ws_extruder = std::to_string(old_extruder) + string(" → ") + std::to_string(extruder.load());

        publish(client, bambu::msg::uload);
        fpr("发送了退料命令,等待退料完成");
        mstd::atomic_wait_un(ams_status, 退料完成需要退线);
        fpr("退料完成,需要退线,等待退线完");

        motor_run(old_extruder, false);// 退线

        mstd::atomic_wait_un(ams_status, 退料完成);// 应该需要这个wait,打印机或者网络偶尔会卡

        fpr("进线");
        old_extruder = extruder;

        motor_run(old_extruder, true);// 进线

        // {//旧的使用进线程序的进料过程
        // 	publish(client,bambu::msg::load);
        // 	fpr("发送了料进线命令,等待进线完成");
        // 	mstd::atomic_wait_un(ams_status,262);
        // 	mstd::delay(2s);
        // 	publish(client,bambu::msg::click_done);
        // 	mstd::delay(2s);
        // 	mstd::atomic_wait_un(ams_status,263);
        // 	publish(client,bambu::msg::click_done);
        // 	mstd::atomic_wait_un(ams_status,进料完成);
        // 	mstd::delay(2s);
        // }

        publish(client, bambu::msg::print_resume);// 暂停恢复
        mstd::delay(4s);// 等待命令落实,这个4s挺准
        esp::gpio_out(config::motors[old_extruder - 1].forward, true);
        mstd::delay(7s);// 辅助进料时间,@_@也可以考虑放在config
        esp::gpio_out(config::motors[old_extruder - 1].forward, false);

        ws_extruder = std::to_string(extruder.load());// 更新前端显示的耗材编号

        pause_lock = false;
    }// while
}// work
/*
 * 似乎外挂托盘的数据也能通过mqtt改动
 */




#include "index.hpp"

volatile bool running_flag{false};

extern "C" void app_main() {

    //微动任务
    std::thread task([]() {
        pinMode(config::forward_click, INPUT_PULLUP);
        attachInterrupt(
            digitalPinToInterrupt(config::forward_click),
            []() {
                running_flag = true;
            },
            FALLING);

        while (true) {
            if (!running_flag) {
                mstd::delay(200ms);
                continue;
            }
            fpr("微动触发");
            motor_run(extruder, true, 1s);
            running_flag = false;
        }
    });




    {// wifi连接部分
        mesp::ConfigStore wificonfig("wificonfig");

        string Wifi_ssid = wificonfig.get("Wifi_ssid", "");
        string Wifi_pass = wificonfig.get("Wifi_pass", "");

        if (Wifi_ssid == "") {
            WiFi.mode(WIFI_AP_STA);
            WiFi.beginSmartConfig();

            while (!WiFi.smartConfigDone()) {
                delay(1000);
                fpr("Waiting for SmartConfig");
            }

            Wifi_ssid = WiFi.SSID().c_str();
            Wifi_pass = WiFi.psk().c_str();

            wificonfig.set("Wifi_ssid", Wifi_ssid);
            wificonfig.set("Wifi_pass", Wifi_pass);
        } else {
            WiFi.begin(Wifi_ssid.c_str(), Wifi_pass.c_str());
        }

        // 等待WiFi连接到路由器
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            fpr("Waiting for WiFi Connected");
        }

        fpr("WiFi Connected to AP");
        fpr("IP Address: ", (int)WiFi.localIP()[0], ".", (int)WiFi.localIP()[1], ".", (int)WiFi.localIP()[2], ".", (int)WiFi.localIP()[3]);
    }// wifi连接部分


    using namespace config;
    using namespace ArduinoJson;
    using std::string;


    std::thread async_thread([]() {
        while (true) {
            auto task = async_channel.pop();
            task();
        }
    });

    std::binary_semaphore mqtt_Signal{0};

    {//服务器配置部分
        server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(200, "text/html", web.c_str());
        });

        // 配置 WebSocket 事件处理
        ws.onEvent([&](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
            if (type == WS_EVT_CONNECT) {
                fpr("WebSocket 客户端", client->id(), "已连接\n");

                JsonDocument doc;
                JsonObject root = doc.to<JsonObject>();
                root.createNestedArray("data");// 创建data数组

                for (auto& [name, to_json] : mesp::ws_value_to_json)
                    to_json(doc);// 添加当前值到data数组
                mesp::sendJson(doc);// 发送所有注册的值

                fpr(doc);
            } else if (type == WS_EVT_DISCONNECT) {
                fpr("WebSocket 客户端 ", client->id(), "已断开\n");
            } else if (type == WS_EVT_DATA) {// 处理接收到的数据
                fpr("收到ws数据");
                data[len] = 0;// 确保字符串终止

                JsonDocument doc;
                deserializeJson(doc, data);
                fpr("ws收到的json\n", doc, "\n");

                // 遍历 data 字段下的所有 jsonobj
                // if (doc["data"].is<JsonArray>()) {
                //     for (JsonObject obj : doc["data"].as<JsonArray>()) {
                //         if (obj["name"].is<const char*>()) {
                if (doc.containsKey("data") && doc["data"].is<JsonArray>()) {
                    for (JsonObject obj : doc["data"].as<JsonArray>()) {
                        if (obj.containsKey("name")) {
                            std::string name = obj["name"].as<std::string>();
                            auto it = mesp::ws_value_update.find(name);
                            if (it != mesp::ws_value_update.end()) {
                                it->second(obj);//更新值
                            }

                            if (name == "device_serial") {//需要连接mqtt,放这里感觉有些耦合
                                mqtt_Signal.release();
                            }
                        }
                    }
                }//if


                if (doc["action"]["command"].as<string>() != "") {
                    std::string command = doc["action"]["command"].as<string>();
                    int moter_id = doc["action"]["moter_id"] | 1;// 默认1
                    if (command == "motor_forward") {
                        async_channel.emplace(
                            [moter_id]() {
                                motor_run(moter_id, true);
                            });
                    } else if (command == "motor_backward") {
                        async_channel.emplace(
                            [moter_id]() {
                                motor_run(moter_id, false);
                            });
                    }
                }//电机控制

            }//WS_EVT_DATA
        });
        server.addHandler(&ws);

        // 设置未找到路径的处理
        server.onNotFound([](AsyncWebServerRequest* request) {
            request->send(404, "text/plain", "404: Not found");
        });

        // 启动服务器
        server.begin();
        fpr("HTTP 服务器已启动");
    }


    {// 打印机Mqtt配置

        if (MQTT_pass != "") {// 有旧数据,可以先连MQTT
            fpr("当前MQTT配置\n", bambu_ip, '\n', MQTT_pass, '\n', device_serial);
            mesp::Mqttclient Mqtt(mqtt_server(bambu_ip), mqtt_username, MQTT_pass, callback_fun);
            webfpr(ws, "MQTT连接中...");
            Mqtt.wait();
            if (Mqtt.connected()) {
                fpr("MQTT连接成功");
                MQTT_done = true;

                work(std::move(Mqtt));// 启动任务,阻塞
            } else {
                MQTT_done = false;
                //Mqtt错误反馈分类
                webfpr(ws, "MQTT连接错误");
            }
        }//if (MQTT_pass != "")

        while (true) {
            fpr("等待Mqtt配置");
            mqtt_Signal.acquire();// 等待mqtt配置
            fpr("当前MQTT配置\n", bambu_ip, '\n', MQTT_pass, '\n', device_serial);
            mesp::Mqttclient Mqtt(mqtt_server(bambu_ip), mqtt_username, MQTT_pass, callback_fun);
            webfpr(ws, "MQTT连接中...");
            Mqtt.wait();
            if (Mqtt.connected()) {
                fpr("MQTT连接成功");
                MQTT_done = true;

                work(std::move(Mqtt));// 启动任务,阻塞
            } else {
                //Mqtt错误反馈分类
                MQTT_done = false;

                webfpr(ws, "MQTT连接错误");
            }
        }
    }// 打印机Mqtt配置



    int cnt = 0;
    while (true) {
        mstd::delay(2000ms);
        // esp::gpio_out(esp::LED_R, cnt % 2);
        ++cnt;
    }
    return;
}