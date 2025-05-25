#pragma once

#include <functional>
#include <map>
#include <string>

#include "esptools.hpp"
#include <ArduinoJson.hpp>
#include <ESPAsyncWebServer.h>


namespace mesp {

    inline std::map<std::string, std::function<void(const JsonObject&)>> ws_value_update;
    inline std::map<std::string, std::function<void(JsonDocument&)>> ws_value_to_json;
    //全局的ws服务
    inline AsyncWebSocket ws_server("/ws");//现在似乎也没有使用非全局的ws的需求,就先统一为这个了
    inline ConfigStore ws_config("ws");

    //发送json到前端
    inline void sendJson(const JsonDocument& doc, AsyncWebSocket& ws = ws_server) {
        String msg;
        serializeJson(doc, msg);
        ws.textAll(msg);
    };

    //与前端的同步类型
    template <typename T>
    struct wsValue {
        //@_@静态断言,T只能基本类型
        using value_type = T;

        const std::string name;//值名,不可改变
        value_type value;

        template <typename... V>
        wsValue(const std::string& n, V&&... v) : name(n), value(std::forward<V>(v)...) {
            ws_value_update[name] = [this](const JsonObject& obj) {
                this->set(obj);
            };
            ws_value_to_json[name] = [this](JsonDocument& doc) {
                this->to_json(doc);
            };
            //注册到全局map中
            //可以考虑加个检查,如果有重复的就报错@_@
            //如果要扩展为非全局,可以给构造加一个入参,成员加一个引用
        }
        ~wsValue() {
            ws_value_update.erase(name);
            ws_value_to_json.erase(name);
        }

        wsValue(const wsValue&) = delete;
        wsValue& operator=(const wsValue&) = delete;
        wsValue(wsValue&&) = delete;
        wsValue& operator=(wsValue&&) = delete;
        //先关掉所有拷贝移动

        operator const value_type&() const noexcept {
            return value;
        }

        void to_json(JsonDocument& doc) const {//非基本类型,就需要特化这个成员函数
            //构造对应的json
            //ArduinoJson想要分块构造貌似只能这样传?
            JsonArray data = doc["data"].as<JsonArray>();
            JsonObject item = data.createNestedObject();
            item["name"] = name;
            item["value"] = value;
        }

        void set(const JsonObject& obj) {//非基本类型,就需要特化这个成员函数
            if (!obj.isNull()) {//为空就只更新前端状态的意思
                value = obj["value"].as<value_type>();
            }
            update();
        }

        void update() const {
            //构造对应的json且发送
            JsonDocument doc;
            JsonObject root = doc.to<JsonObject>();
            root.createNestedArray("data");// 创建data数组

            to_json(doc);// 添加当前值到data数组
            sendJson(doc);
        }
        //自然,这种每个元素只发自己的json方式,在网络IO上称不上高效,不过写起来比较方便


        wsValue& operator=(const value_type& v) {
            value = v;

            update();
            return *this;
        }
        wsValue& operator=(value_type&& v) noexcept {
            value = std::move(v);
            update();
            return *this;
        }


        bool operator==(const value_type& v) const noexcept {
            return value == v;
        }
    };//ws_value
    
    template <typename T>
    inline std::ostream& operator<<(std::ostream& os, const wsValue<T>& v) {
        return os << v.value;
    }


    //与前端的同步类型,会写硬盘
    template <typename T>
    struct wsStoreValue : wsValue<T> {

        using typename wsValue<T>::value_type;
        using wsValue<T>::name;
        using wsValue<T>::value;
        using wsValue<T>::update;

        template <typename... V>
        wsStoreValue(const std::string& n, V&&... v) : wsValue<T>(n, std::forward<V>(v)...) {
            // 从配置中加载初始值，如果没有则使用传入的默认值
            value = ws_config.get(name, value);
        }


        void set(const JsonObject& obj) {//非基本类型,就需要特化这个成员函数
            if (!obj.isNull()) {//为空就只更新前端状态的意思
                value = obj["value"].as<value_type>();
                ws_config.set(name, value);
            }
            update();
        }


        wsStoreValue& operator=(const value_type& v) {
            value = v;
            ws_config.set(name, value);

            update();
            return *this;
        }
        wsStoreValue& operator=(value_type&& v) noexcept {
            value = std::move(v);
            ws_config.set(name, value);

            update();
            return *this;
        }

    };//ws_value



}//mesp

/*
和前端值更新思路
更新时,初始化时,要发给前端的json
接受到前端的json时的更改
    需要一个地方统一处理,根据接受到json的名,来选对应的赋值函数
    T要有自己的json处理函数
    写一个适用基础类型的
    然后特化每个类的
    json格式
    value_name 
    
*/