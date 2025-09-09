#pragma once
#include "esp_heap_caps.h"
#include "tools.hpp"
#include <Arduino.h>
#include <Preferences.h>

#include <WiFi.h>


namespace mesp {

    struct init_t {
        init_t() {
            initArduino();
        }
    };
    inline init_t __init;

    struct ConfigStore {
        Preferences prefs;

        ConfigStore(const std::string_view name) {
            prefs.begin(name.data(), false);
        }
        ~ConfigStore() {
            prefs.end();
        }

        void set(const std::string_view key, const std::string_view value) {
            prefs.putString(key.data(), value.data());
        }
        void set(const std::string_view key, const char* value) {
            prefs.putString(key.data(), value);
        }
        void set(const std::string_view key, const int value) {
            prefs.putInt(key.data(), value);
        }
        void set(const std::string_view key, const float value) {
            prefs.putFloat(key.data(), value);
        }
        void set(const std::string_view key, const bool value) {
            prefs.putBool(key.data(), value);
        }

        template <typename T>
        T get(const std::string_view ker, const T& def = T()) {
            if constexpr (std::is_same_v<T, int>)
                return prefs.getInt(ker.data(), def);
            else if constexpr (std::is_same_v<T, float>)
                return prefs.getFloat(ker.data(), def);
            else if constexpr (std::is_same_v<T, bool>)
                return prefs.getBool(ker.data(), def);
            else if constexpr (std::is_same_v<T, std::string>)
                return prefs.getString(ker.data(), def.c_str()).c_str();
            // else if constexpr (std::is_same_v<T, const char*>)
            //     return prefs.getString(key.c_str(), def).c_str();//这个要给返回类型加上元函数,比较麻烦
            else
                static_assert(false, "Unsupported type for ConfigStore get");
        }

        std::string get(const std::string_view key, const char* def) {
            fpr("ker", "get");
            fpr(def);
            return prefs.getString(key.data(), def).c_str();
        }


        void remove(const std::string_view key) {
            prefs.remove(key.data());
        }

        void clear() {
            prefs.clear();
        }
    };// Preferences_type


    //打印内存信息
    inline void print_memory_info() {
        fpr("当前可用内存: ", esp_get_free_heap_size(), "字节\n");
        fpr("历史最小可用内存: ", esp_get_minimum_free_heap_size(), "字节\n");

        // 详细堆信息（按内存类型分类）
        // heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
    }
}// mesp