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

        ConfigStore(const std::string &name) {
            prefs.begin(name.c_str(), false);
        }
        ~ConfigStore() {
            prefs.end();
        }

        void set(const std::string &key, const std::string &value) {
            prefs.putString(key.c_str(), value.c_str());
        }
        void set(const std::string &key, const char *value) {
            prefs.putString(key.c_str(), value);
        }
        void set(const std::string &key, const int &value) {
            prefs.putInt(key.c_str(), value);
        }
        void set(const std::string &key, const float &value) {
            prefs.putFloat(key.c_str(), value);
        }
        void set(const std::string &key, const bool &value) {
            prefs.putBool(key.c_str(), value);
        }

        std::string get(const std::string &key, const std::string &def) {
            return prefs.getString(key.c_str(), def.c_str()).c_str();
        }
        std::string get(const std::string &key, const char *def) {
            return prefs.getString(key.c_str(), def).c_str();
        }
        int get(const std::string &key, int def) {
            return prefs.getInt(key.c_str(), def);
        }
        float get(const std::string &key, float def) {
            return prefs.getFloat(key.c_str(), def);
        }
        bool get(const std::string &key, bool def) {
            return prefs.getBool(key.c_str(), def);
        }

        void remove(const std::string &key) {
            prefs.remove(key.c_str());
        }

        void clear() {
            prefs.clear();
        }
    }; // Preferences_type
} // esp