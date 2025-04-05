#pragma once
#include "config.hpp"
#include "tools.hpp"

#include "espIO.hpp"
#include "esp_ota_ops.h"
#include "esp_tls.h"
#include "mqtt_client.h"

namespace esp {

    using callback_fun_ptr = void (*)(esp_mqtt_client_handle_t, const std::string &);

    template <callback_fun_ptr f>
    void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
        auto TAG = "MQTT ";
        fpr("\n事件分发");
        esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
        esp_mqtt_client_handle_t client = event->client;
        int msg_id = -1;
        switch (esp_mqtt_event_id_t(event_id)) {
        case MQTT_EVENT_CONNECTED:
            fpr(TAG, "MQTT_EVENT_CONNECTED（MQTT连接成功）");
            // msg_id = esp_mqtt_client_subscribe(client, config::topic_subscribe(config::device_serial).c_str(), 1); // 这里不应该从config读@_@
            msg_id = esp_mqtt_client_subscribe(client, config::topic_subscribe().c_str(), 1); // 这里不应该从config读@_@
            fpr(TAG, "发送订阅成功，msg_id=", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            fpr(TAG, "MQTT_EVENT_DISCONNECTED（MQTT断开连接）");
            break;
        case MQTT_EVENT_BEFORE_CONNECT:
            fpr("连接前");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            fpr(TAG, "MQTT_EVENT_SUBSCRIBED（MQTT订阅成功），msg_id=", event->msg_id);
            esp::gpio_out(esp::LED_L, true);
            mstd::delay(1s);
            esp::gpio_out(esp::LED_L, false);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            fpr(TAG, "MQTT_EVENT_UNSUBSCRIBED（MQTT取消订阅成功），msg_id=", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            fpr(TAG, "MQTT_EVENT_PUBLISHED（MQTT消息发布成功），msg_id=", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            // fpr(TAG,"MQTT_EVENT_DATA（接收到MQTT消息）");
            // printf("主题=%.*s\r\n",event->topic_len,event->topic);
            printf("%.*s\r\n", event->data_len, event->data);
            // fpr(std::string(event->data));

            // 可以加约束,约束F分派
            f(client, std::string(event->data));
            break;
        case MQTT_EVENT_ERROR:
            fpr(TAG, "MQTT_EVENT_ERROR（MQTT事件错误）");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                fpr(TAG, "从esp-tls报告的最后错误代码：", event->error_handle->esp_tls_last_esp_err);
                fpr(TAG, "TLS堆栈最后错误号：", event->error_handle->esp_tls_stack_err);
                fpr(TAG, "最后捕获的errno：", event->error_handle->esp_transport_sock_errno,
                    strerror(event->error_handle->esp_transport_sock_errno));
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                fpr(TAG, "连接被拒绝错误：", event->error_handle->connect_return_code);
            } else {
                fpr(TAG, "未知的错误类型：", event->error_handle->error_type);
            }
            break;
        default:
            fpr(TAG, "其他事件id:", event->event_id);
            break;
        }
    } // mqtt_event_handler


    template <callback_fun_ptr f>
    [[nodiscard]] esp_mqtt_client_handle_t mqtt_app_start(const std::string server, const std::string &user, const std::string &pass) {
        esp_mqtt_client_config_t mqtt_cfg{};
        mqtt_cfg.broker.address.uri = server.c_str();
        // mqtt_cfg.broker.verification.use_global_ca_store = false;
        mqtt_cfg.broker.verification.skip_cert_common_name_check = true;
        mqtt_cfg.credentials.username = user.c_str();
        mqtt_cfg.credentials.authentication.password = pass.c_str();
        // mqtt_cfg.network.disable_auto_reconnect = true;

        fpr("[APP] Free memory: ", esp_get_free_heap_size(), "bytes");
        esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
        /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */

        esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler<f>, nullptr);
        esp_mqtt_client_start(client);

        // delay(100);
        // esp_mqtt_client_stop(client);
        // fpr("MQTT停止连接");
        return client;
    }

} // namespace esp


namespace mesp {

    using std::string;

    using callback_fun_ptr = void (*)(esp_mqtt_client_handle_t, const string &);

    struct Mqttclient {
      private:
        static void mqtt_event_callback(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
            fpr("\nMqtt事件分发");
            esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
            esp_mqtt_client_handle_t client = event->client;
            // int msg_id = -1;

            Mqttclient &This = *static_cast<Mqttclient *>(handler_args);
            callback_fun_ptr f = This.event_data_fun;

            switch (esp_mqtt_event_id_t(event_id)) {
            case MQTT_EVENT_CONNECTED:
                fpr("MQTT_EVENT_CONNECTED（MQTT连接成功）");
                This.state = mqtt_state::connected;
                This.state.notify_all();
                break;
            case MQTT_EVENT_DISCONNECTED:
                fpr("MQTT_EVENT_DISCONNECTED（MQTT断开连接）");
                This.state = mqtt_state::disconnected;
                This.state.notify_all();
                break;
            case MQTT_EVENT_BEFORE_CONNECT:
                fpr("Mqtt连接前");
                break;
            case MQTT_EVENT_SUBSCRIBED:
                fpr("MQTT_EVENT_SUBSCRIBED（MQTT订阅成功），msg_id=", event->msg_id);
                break;
            case MQTT_EVENT_UNSUBSCRIBED:
                fpr("MQTT_EVENT_UNSUBSCRIBED（MQTT取消订阅成功），msg_id=", event->msg_id);
                break;
            case MQTT_EVENT_PUBLISHED:
                fpr("MQTT_EVENT_PUBLISHED（MQTT消息发布成功），msg_id=", event->msg_id);
                break;
            case MQTT_EVENT_DATA:
                fpr("MQTT_EVENT_DATA（接收到MQTT消息）");
                // printf("主题=%.*s\r\n",event->topic_len,event->topic);
                printf("%.*s\r\n", event->data_len, event->data);
                f(client, string(event->data));
                break;
            case MQTT_EVENT_ERROR:
                fpr("MQTT_EVENT_ERROR（MQTT事件错误）");
                if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                    fpr("从esp-tls报告的最后错误代码：", event->error_handle->esp_tls_last_esp_err);
                    fpr("TLS堆栈最后错误号：", event->error_handle->esp_tls_stack_err);
                    fpr("最后捕获的errno：", event->error_handle->esp_transport_sock_errno,
                        strerror(event->error_handle->esp_transport_sock_errno));
                } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                    fpr("连接被拒绝错误：", event->error_handle->connect_return_code);
                } else {
                    fpr("未知的错误类型：", event->error_handle->error_type);
                }
                This.state = mqtt_state::error;
                This.state.notify_all();
                break;
            default:
                fpr("其他事件id:", event->event_id);
                break;
            }
        } // mqtt_event_callback
      public:
        esp_mqtt_client_handle_t client = nullptr;
        const callback_fun_ptr event_data_fun;

        struct mqtt_state {
            constexpr static int init = 0;
            constexpr static int destroy = 1;
            constexpr static int connected = 2;
            constexpr static int disconnected = 3;
            constexpr static int error = 4;
        }; // mqtt_state

        std::atomic<int> state = mqtt_state::init;

        // 连接成功
        // esp_event_handler_t event_connected = [](void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {};
        // // 断开连接
        // esp_event_handler_t event_diconnected = [](void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {};
        // // 订阅成功
        // esp_event_handler_t event_subscribed = [](void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {};

        Mqttclient(const string &server, const string &user, const string &pass, callback_fun_ptr f)
            : event_data_fun(f) {
            esp_mqtt_client_config_t mqtt_cfg{};
            mqtt_cfg.broker.address.uri = server.c_str();
            // mqtt_cfg.broker.verification.use_global_ca_store = false;
            mqtt_cfg.broker.verification.skip_cert_common_name_check = true;
            mqtt_cfg.credentials.username = user.c_str();
            mqtt_cfg.credentials.authentication.password = pass.c_str();
            // mqtt_cfg.network.disable_auto_reconnect = true;//自动重连

            client = esp_mqtt_client_init(&mqtt_cfg);

            error_check(esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, Mqttclient::mqtt_event_callback, this), "Mqtt注册事件失败");
            error_check(esp_mqtt_client_start(client), "Mqtt初始化失败");
        }
        ~Mqttclient() {
            if (state != mqtt_state::init)
                error_check(esp_mqtt_client_stop(client));
            error_check(esp_mqtt_client_destroy(client));
        }
        Mqttclient(const Mqttclient &) = delete;
        Mqttclient &operator=(const Mqttclient &) = delete;
        Mqttclient(Mqttclient &&r) noexcept
            : event_data_fun(r.event_data_fun) {
            client = r.client;
            state.store(r.state);
            r.client = nullptr;
            r.state = mqtt_state::init;
        }
        Mqttclient &operator=(Mqttclient &&) noexcept = delete;
        // 先都delete,不然还要加引用计数

        operator esp_mqtt_client_handle_t &() noexcept {
            return client;
        }
        operator const esp_mqtt_client_handle_t &() const noexcept {
            return client;
        }

        // 订阅主题
        void subscribe(const string &topic, int Qos = 1) {
            esp_mqtt_client_subscribe(client, topic.c_str(), Qos);
        }

        void error_check(esp_err_t err, const string &msg = "MQTT错误:") {
            if (err != ESP_OK) {
                fpr(msg, err);
                state.store(mqtt_state::error);
                state.notify_all();
            }
        }

    }; // Mqttclinet




} // mesp