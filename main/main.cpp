#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
// #include <ESPUI.h>
// #include <semaphore>

// #include <esp_mq

#include "esptools.hpp"

#if __has_include("localconfig.hpp")
#include "localconfig.hpp"
#endif

#include "bambu.hpp"
#include "espIO.hpp"
#include "espMQTT.hpp"

using std::string;


int bed_target_temper_max = 0;
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


void publish(esp_mqtt_client_handle_t client, const std::string &msg) {
    esp::gpio_out(esp::LED_L, true);
    // mstd::delay(2s);
    fpr("发送消息:", msg);
    int msg_id = esp_mqtt_client_publish(client, config::topic_publish().c_str(), msg.c_str(), msg.size(), 0, 0);
    if (msg_id < 0)
        fpr("发送失败");
    else
        fpr("发送成功,消息id=", msg_id);
    // fpr(TAG, "binary sent with msg_id=%d", msg_id);
    esp::gpio_out(esp::LED_L, false);
    mstd::delay(2s);//@_@这些延时还可以调整看看
}

void callback_fun(esp_mqtt_client_handle_t client, const std::string &json) {// 接受到信息的回调
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
            mstd::delay(4500ms);            // 貌似4s还是有可能会哟ubug
            if (bed_target_temper_max > 0) {// 似乎热床置零会导致热端固定到90
                publish(client, bambu::msg::runGcode(
                                    std::string("M190 S") + std::to_string(bed_target_temper_max)// 恢复原来的热床温度
                                    // + std::string(R"(\nM109 S255)")//提前升温,9系命令自带阻塞,应该无法使两条一起生效
                                    ));
            }

            if (extruder.exchange(bed_target_temper) != bed_target_temper) {
                fpr("唤醒换料程序");
                pause_lock = true;
                extruder.notify_one();      // 唤醒耗材切换
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
    int old_extruder = extruder;
    while (true) {

        esp::gpio_out(esp::LED_R, true);
        fpr("等待换料");
        extruder.wait(old_extruder);
        esp::gpio_out(esp::LED_R, false);

        publish(client, bambu::msg::uload);
        fpr("发送了退料命令,等待退料完成");
        mstd::atomic_wait_un(ams_status, 退料完成需要退线);
        fpr("退料完成,需要退线,等待退线完");

        esp::gpio_out(config::motors[old_extruder - 1].backward, true);
        mstd::delay(config::uload_time);
        // 这里可以检查一下线确实退出来了
        esp::gpio_out(config::motors[old_extruder - 1].backward, false);
        mstd::atomic_wait_un(ams_status, 退料完成);// 应该需要这个wait,打印机或者网络偶尔会卡

        fpr("进线");
        old_extruder = extruder;
        esp::gpio_out(config::motors[old_extruder - 1].forward, true);
        mstd::delay(config::load_time);
        esp::gpio_out(config::motors[old_extruder - 1].forward, false);

        // {
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
        mstd::delay(4s);                          // 等待命令落实
        esp::gpio_out(config::motors[old_extruder - 1].forward, true);
        mstd::delay(7s);// 辅助进料时间,@_@也可以考虑放在config
        esp::gpio_out(config::motors[old_extruder - 1].forward, false);

        pause_lock = false;
    }// while
}// work
/*
 * 似乎外挂托盘的数据也能通过mqtt改动
 */


AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const std::string web = R"rawliteral(
<!DOCTYPE html><html lang="zh-CN"><head><meta charset="UTF-8"><meta name="viewport"content="width=device-width, initial-scale=1.0"><title>AMS控制台</title><style>:root{--primary-color:#2c3e50;--secondary-color:#3498db;--success-color:#27ae60;--danger-color:#e74c3c}body{margin:0;font-family:'Segoe UI',system-ui;background:linear-gradient(135deg,#f5f7fa 0%,#c3cfe2 100%);color:#333}.status-bar{display:flex;justify-content:space-between;padding:12px 16px;background:rgba(255,255,255,0.95);box-shadow:0 2px 10px rgba(0,0,0,0.1);font-size:14px}.control-panel{max-width:480px;margin:20px auto;background:rgba(255,255,255,0.98);border-radius:12px;box-shadow:0 8px 24px rgba(0,0,0,0.1);padding:24px;box-sizing:border-box}h3{margin-top:0;color:var(--primary-color);border-bottom:1px solid#eee;padding-bottom:12px}.channel-list{margin:16px 0}.channel-item{display:flex;align-items:center;padding:12px;margin:8px 0;background:#fff;border-radius:8px;box-shadow:0 2px 6px rgba(0,0,0,0.05);transition:all 0.2s}.channel-item:hover{box-shadow:0 4px 8px rgba(0,0,0,0.1)}.color-indicator{width:24px;height:24px;border-radius:50%;margin-right:12px;border:1px solid rgba(0,0,0,0.1)}.channel-info{flex:1}.channel-name{font-weight:500}.channel-material{font-size:0.9em;color:#666}.auto-reload-switch{position:relative;display:inline-block;width:48px;height:24px;margin-left:12px}.auto-reload-slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ccc;transition:.4s;border-radius:24px}.auto-reload-slider:before{position:absolute;content:"";height:18px;width:18px;left:3px;bottom:3px;background-color:white;transition:.4s;border-radius:50%}input:checked+.auto-reload-slider{background-color:var(--success-color)}input:checked+.auto-reload-slider:before{transform:translateX(24px)}.input-group{margin-bottom:16px}.input-group label{display:block;margin-bottom:6px;font-size:14px;color:#555}.input-group input,.input-group select{width:100%;padding:10px;border:1px solid#ddd;border-radius:6px;font-size:14px;box-sizing:border-box}.action-buttons{display:grid;grid-template-columns:repeat(2,1fr);gap:12px;margin-top:24px}.action-btn{padding:12px;background:var(--secondary-color);color:white;border:none;border-radius:8px;font-size:14px;cursor:pointer;transition:all 0.2s;text-align:center}.action-btn:hover{opacity:0.9}.action-btn:active{transform:scale(0.98)}.action-btn.danger{background:var(--danger-color)}.printer-input-group{display:flex;gap:8px;margin:16px 0}.printer-input{flex:1;padding:10px;border:1px solid#ddd;border-radius:6px;font-size:14px}.send-btn{padding:10px 16px;background:var(--success-color);color:white;border:none;border-radius:6px;cursor:pointer;font-size:14px}.log-panel{margin-top:24px;background:#f9f9f9;border-radius:8px;padding:16px}.log-content{height:200px;overflow-y:auto;background:#fff;border:1px solid#eee;border-radius:4px;padding:8px;font-family:monospace;font-size:13px}.log-entry{margin-bottom:4px;line-height:1.4}.hidden{display:none}.tab-buttons{display:flex;margin-bottom:16px;border-bottom:1px solid#eee}.tab-btn{padding:8px 16px;background:none;border:none;border-bottom:2px solid transparent;cursor:pointer;font-size:14px}.tab-btn.active{border-bottom-color:var(--secondary-color);color:var(--secondary-color);font-weight:500}.status-icon{margin:0 8px}.connected{color:#00ff00}.disconnected{color:#ff0000}</style></head><body><div class="status-bar"><!--<span id="connection-status">未连接</span>--><div><span id="ESP-dot"class="status-icon disconnected">●</span><span id="wifi-status">ESP</span></div><div><span id="mqtt-dot"class="status-icon disconnected">●</span><span id="mqtt-status">MQTT</span></div></div><div class="control-panel"><div class="tab-buttons"><button class="tab-btn active"onclick="showTab('config-panel')">打印机配置</button><button class="tab-btn"onclick="showTab('main-panel')">主控制</button><button class="tab-btn"onclick="showTab('channel-panel')">通道管理</button></div><div id="config-panel"><!--<h3>网络配置</h3>--><div class="input-group"><label>打印机ip地址</label><input type="text"id="bambu-ip"placeholder="192.168.1.1"></div><div class="input-group"><label>MQTT密码</label><input type="text"id="mqtt-pass"placeholder="输入MQTT密码"></div><div class="input-group"><label>设备序列号</label><input type="text"id="device-serial"placeholder="XXXXXXXXXXXXXXX"><!--可以加一个如何查看设备序列号的说明@_@--></div><div class="action-buttons"><button class="action-btn"onclick="sendMQTT()">连接打印机</button><!--<button class="action-btn"onclick="showTab('main-panel')">取消</button>--><button class="action-btn"onclick="testfun()">取消</button></div></div><div id="main-panel"class="hidden"></div><div id="channel-panel"class="hidden"></div></div><script>const ws=new WebSocket('ws://'+window.location.hostname+'/ws');const ESP_dot=document.getElementById('ESP-dot');const mqtt_dot=document.getElementById('mqtt-dot');let currentChannels=[];ws.onopen=function(){ESP_dot.classList.replace('disconnected','connected')};ws.onclose=function(){ESP_dot.classList.replace('connected','disconnected')};ws.onmessage=function(event){const data=JSON.parse(event.data);if(data.MQTT){if(data.MQTT.done==true){mqtt_dot.classList.replace('disconnected','connected')}else{mqtt_dot.classList.replace('connected','disconnected')}const bambu_ip=document.getElementById('bambu-ip');bambu_ip.value=data.MQTT.bambu_ip;const mqtt_pass=document.getElementById('mqtt-pass');mqtt_pass.value=data.MQTT.Mqtt_pass;const device_serial=document.getElementById('device-serial');device_serial.value=data.MQTT.device_serial}};ws.onerror=function(error){};function showTab(tabId){document.querySelectorAll('.tab-btn').forEach(btn=>{btn.classList.remove('active')});event.target.classList.add('active');document.getElementById('main-panel').classList.add('hidden');document.getElementById('config-panel').classList.add('hidden');document.getElementById('channel-panel').classList.add('hidden');document.getElementById(tabId).classList.remove('hidden')}function sendMQTT(){const doc={MQTT:{bambu_ip:document.getElementById('bambu-ip').value,Mqtt_pass:document.getElementById('mqtt-pass').value,device_serial:document.getElementById('device-serial').value}};ws.send(JSON.stringify(doc));showTab('main-panel')}function testfun(){ESP_dot.classList.replace('disconnected','connected')}</script></body></html>
)rawliteral";

extern "C" void app_main() {


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
        fpr("IP Address: ", WiFi.localIP()[0], ".", WiFi.localIP()[1], ".", WiFi.localIP()[2], ".", WiFi.localIP()[3]);
    }// wifi连接部分


    using namespace config;
    using namespace ArduinoJson;
    using std::string;

    mesp::ConfigStore Mqttconfig("Mqttconfig");

    bambu_ip = Mqttconfig.get("Mqtt_ip", "192.168.1.1");
    Mqtt_pass = Mqttconfig.get("Mqtt_pass", "");
    device_serial = Mqttconfig.get("device_serial", "");
    std::atomic<bool> mqtt_done = false;

    auto sendMQTT = [&](AsyncWebSocket &ws) {
        JsonDocument doc;
        // fpr("mqtt_done:", mqtt_done);
        doc["MQTT"]["done"] = mqtt_done;
        doc["MQTT"]["bambu_ip"] = bambu_ip;
        doc["MQTT"]["Mqtt_pass"] = Mqtt_pass;
        doc["MQTT"]["device_serial"] = device_serial;
        String msg;
        serializeJson(doc, msg);
        ws.textAll(msg);
    };



    if (Mqtt_pass != "") {// 有旧数据,可以先连MQTT
        mesp::Mqttclient Mqtt(mqtt_server(bambu_ip), mqtt_username, device_serial, callback_fun);
        Mqtt.wait();
        if (Mqtt.connected()) {
            work(std::move(Mqtt));
            mqtt_done = true;
            fpr("MQTT连接成功");
        }
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", web.c_str());
    });

    // 配置 WebSocket 事件处理
    ws.onEvent([&](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            fpr("WebSocket 客户端", client->id(), "已连接\n");
            sendMQTT(ws);
        } else if (type == WS_EVT_DISCONNECT) {
            fpr("WebSocket 客户端 ", client->id(), "已断开\n");
        } else if (type == WS_EVT_DATA) {// 处理接收到的数据

            data[len] = 0;// 确保字符串终止

            JsonDocument doc;
            deserializeJson(doc, data);

            if (doc.containsKey("MQTT")) {
                bambu_ip = doc["MQTT"]["bambu_ip"].as<string>();
                Mqtt_pass = doc["MQTT"]["Mqtt_pass"].as<string>();
                device_serial = doc["MQTT"]["device_serial"].as<string>();
                mqtt_done = true;
                mqtt_done.notify_all();
            }// MQTT
        }
    });
    server.addHandler(&ws);

    // 设置未找到路径的处理
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "404: Not found");
    });

    // 启动服务器
    server.begin();
    fpr("HTTP 服务器已启动");


    {// Mqtt和打印机配置
        while (true) {
            mqtt_done.wait(false);

            fpr("当前MQTT配置", bambu_ip, '\n', Mqtt_pass, '\n', device_serial);
            mesp::Mqttclient Mqtt(mqtt_server(bambu_ip), mqtt_username, device_serial, callback_fun);
            Mqtt.wait();
            if (Mqtt.connected()) {
                Mqttconfig.set("Mqtt_ip", bambu_ip);
                Mqttconfig.set("Mqtt_pass", Mqtt_pass);
                Mqttconfig.set("device_serial", device_serial);
                fpr("MQTT配置已保存");
                sendMQTT(ws);
                work(std::move(Mqtt));// 启动任务
                break;
            } else
                mqtt_done = false;
        }
    }// Mqtt配置

    int cnt = 0;
    while (true) {
        mstd::delay(2000ms);
        // esp::gpio_out(esp::LED_R, cnt % 2);
        ++cnt;
    }

    return;
}