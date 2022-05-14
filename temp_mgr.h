
#ifndef TEMP_MGR_H
#define TEMP_MGR_H

#include <RTClib.h>

#include "settings.h"

#define HEAT_PIN 11
#define COOL_PIN 12
#define FAN_PIN 13

#define TEMP_THRESHOLD 0.5

class TempMgr {
    public:
    TempMgr(Settings* settings, RTC_DS1307* rtc) {
        this->settings = settings;
        this->rtc = rtc;
        last_called = 0;
    }
    TempMgr(const TempMgr& tmgr) {
        settings = tmgr.settings;
        rtc = tmgr.rtc;
        last_called = 0;
    }
    // Updates whether the thermostat is currently calling for heat, cooling, the fan, or neither
    void update_call(float current_temp) {
        // Set the temperature mode to simple if the RTC isn't running
        TempSetting* tgt_temp;
        if (!rtc->isrunning()) {
            settings->control_mode = MODE_SIMPLE;
            tgt_temp = settings->get_current_setting(NULL);
        }
        else {
            DateTime now = rtc->now();
            TempSetting* tgt_temp = settings->get_current_setting(&now);
        }

        if (settings->mode == Mode::Off) {
            digitalWrite(HEAT_PIN, LOW);
            digitalWrite(COOL_PIN, LOW);
            digitalWrite(FAN_PIN, LOW);
        }
        else if (settings->mode == Mode::Fan) {
            digitalWrite(HEAT_PIN, LOW);
            digitalWrite(COOL_PIN, LOW);
            digitalWrite(FAN_PIN, HIGH);
        }
        else if (settings->mode == Mode::Heat) {
            if (current_temp < tgt_temp->target_temp() - TEMP_THRESHOLD) {
                digitalWrite(HEAT_PIN, HIGH);
                digitalWrite(FAN_PIN, HIGH);
            }
            else if (current_temp > tgt_temp->target_temp() + TEMP_THRESHOLD) {
                digitalWrite(HEAT_PIN, LOW);
                digitalWrite(FAN_PIN, LOW);
            }
            digitalWrite(COOL_PIN, LOW);
        }
        else if (settings->mode == Mode::Cool) {
            if (current_temp > tgt_temp->target_temp() + TEMP_THRESHOLD) {
                digitalWrite(COOL_PIN, HIGH);
                digitalWrite(FAN_PIN, HIGH);
            }
            else if (current_temp < tgt_temp->target_temp() - TEMP_THRESHOLD) {
                digitalWrite(COOL_PIN, LOW);
                digitalWrite(FAN_PIN, LOW);
            }
            digitalWrite(HEAT_PIN, LOW);
        }
        else if (settings -> mode == Mode::Auto) {
            if (digitalRead(HEAT_PIN)) {
                if (current_temp > tgt_temp->target_temp()) {
                    digitalWrite(HEAT_PIN, LOW);
                    digitalWrite(FAN_PIN, LOW);
                }
            }
            if (digitalRead(COOL_PIN)) {
                if (current_temp < tgt_temp->target_temp()) {
                    digitalWrite(COOL_PIN, LOW);
                    digitalWrite(FAN_PIN, LOW);
                }
            }
            else {
                if (current_temp < tgt_temp->target_temp() - TEMP_THRESHOLD) {
                    digitalWrite(HEAT_PIN, HIGH);
                    digitalWrite(FAN_PIN, HIGH);
                }
                if (current_temp > tgt_temp->target_temp() + TEMP_THRESHOLD) {
                    digitalWrite(COOL_PIN, HIGH);
                    digitalWrite(FAN_PIN, HIGH);
                }
            }
        }
    }
    /*
    If 2000ms has passed since the last call of update_call, calls update_call and returns true
    otherwise returns false
    */
    bool update_call_timer(float current_temp) {
        if (millis() - last_called < 2000) {
            return false;
        }
        update_call(current_temp);
        last_called = millis();
        return true;
    }
    bool is_running() {
        return (digitalRead(FAN_PIN) || digitalRead(COOL_PIN) || digitalRead(HEAT_PIN));
    }
    private:
    Settings* settings;
    RTC_DS1307* rtc;
    unsigned long last_called;
};

#endif
