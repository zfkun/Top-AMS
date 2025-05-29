#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#include "tools.hpp"


namespace mesp {

    struct init_t {
        init_t() {
            initArduino();
        }
    };
    inline init_t __init;

    struct ConfigStore {
        Preferences prefs;

        ConfigStore(const std::string& name) {
            prefs.begin(name.c_str(), false);
        }
        ~ConfigStore() {
            prefs.end();
        }

        void set(const std::string& key, const std::string& value) {
            prefs.putString(key.c_str(), value.c_str());
        }
        void set(const std::string& key, const char* value) {
            prefs.putString(key.c_str(), value);
        }
        void set(const std::string& key, const int& value) {
            prefs.putInt(key.c_str(), value);
        }
        void set(const std::string& key, const float& value) {
            prefs.putFloat(key.c_str(), value);
        }
        void set(const std::string& key, const bool& value) {
            prefs.putBool(key.c_str(), value);
        }

        template <typename T>
        T get(const std::string& ker, const T& def = T()) {
            if constexpr (std::is_same_v<T, int>)
                return prefs.getInt(ker.c_str(), def);
            else if constexpr (std::is_same_v<T, float>)
                return prefs.getFloat(ker.c_str(), def);
            else if constexpr (std::is_same_v<T, bool>)
                return prefs.getBool(ker.c_str(), def);
            else if constexpr (std::is_same_v<T, std::string>)
                return prefs.getString(ker.c_str(), def.c_str()).c_str();
            // else if constexpr (std::is_same_v<T, const char*>)
            //     return prefs.getString(key.c_str(), def).c_str();//这个要给返回类型加上元函数,比较麻烦
            else
                static_assert(false, "Unsupported type for ConfigStore get");
        }

        std::string get(const std::string& key, const char* def) {
            fpr("ker", "get");
            fpr(def);
            return prefs.getString(key.c_str(), def).c_str();
        }


        void remove(const std::string& key) {
            prefs.remove(key.c_str());
        }

        void clear() {
            prefs.clear();
        }
    };// Preferences_type
}// esp