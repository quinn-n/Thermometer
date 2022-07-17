
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
    // Returns `true` if the call changes
    bool update_call(float current_temp) {
        // Set the temperature mode to simple if the RTC isn't running
        TempSetting* tgt_temp;
        Mode old_mode = running_mode;
        if (!rtc->isrunning()) {
            settings->control_mode = ControlMode::Simple;
            tgt_temp = settings->get_current_setting(NULL);
        }
        else {
            DateTime now = rtc->now();
            tgt_temp = settings->get_current_setting(&now);
        }

        if (settings->mode == Mode::Off) {
            digitalWrite(HEAT_PIN, LOW);
            digitalWrite(COOL_PIN, LOW);
            digitalWrite(FAN_PIN, LOW);
            running_mode = Mode::Off;
        }
        else if (settings->mode == Mode::Fan) {
            digitalWrite(HEAT_PIN, LOW);
            digitalWrite(COOL_PIN, LOW);
            digitalWrite(FAN_PIN, HIGH);
            running_mode = Mode::Fan;
        }
        else if (settings->mode == Mode::Heat) {
            if (current_temp < tgt_temp->target_temp() - TEMP_THRESHOLD) {
                digitalWrite(HEAT_PIN, HIGH);
                digitalWrite(FAN_PIN, HIGH);
                running_mode = Mode::Heat;
            }
            else if (current_temp > tgt_temp->target_temp() + TEMP_THRESHOLD) {
                digitalWrite(HEAT_PIN, LOW);
                digitalWrite(FAN_PIN, LOW);
                running_mode = Mode::Off;
            }
            digitalWrite(COOL_PIN, LOW);
        }
        else if (settings->mode == Mode::Cool) {
            if (current_temp > tgt_temp->target_temp() + TEMP_THRESHOLD) {
                digitalWrite(COOL_PIN, HIGH);
                digitalWrite(FAN_PIN, HIGH);
                running_mode = Mode::Cool;
            }
            else if (current_temp < tgt_temp->target_temp() - TEMP_THRESHOLD) {
                digitalWrite(COOL_PIN, LOW);
                digitalWrite(FAN_PIN, LOW);
                running_mode = Mode::Off;
            }
            digitalWrite(HEAT_PIN, LOW);
        }
        else if (settings->mode == Mode::Auto) {
            if (running_mode == Mode::Heat) {
                if (current_temp > tgt_temp->target_temp()) {
                    digitalWrite(HEAT_PIN, LOW);
                    digitalWrite(FAN_PIN, LOW);
                    running_mode = Mode::Off;
                }
            }
            else if (running_mode == Mode::Cool) {
                if (current_temp < tgt_temp->target_temp()) {
                    digitalWrite(COOL_PIN, LOW);
                    digitalWrite(FAN_PIN, LOW);
                    running_mode = Mode::Off;
                }
            }
            else {
                if (current_temp < tgt_temp->target_temp() - TEMP_THRESHOLD) {
                    digitalWrite(HEAT_PIN, HIGH);
                    digitalWrite(FAN_PIN, HIGH);
                    running_mode = Mode::Heat;
                }
                if (current_temp > tgt_temp->target_temp() + TEMP_THRESHOLD) {
                    digitalWrite(COOL_PIN, HIGH);
                    digitalWrite(FAN_PIN, HIGH);
                    running_mode = Mode::Cool;
                }
            }
        }

        return running_mode != old_mode;
    }
    /*
    If 2000ms has passed since the last call of update_call, calls update_call and returns true
    otherwise returns false
    */
    bool update_call_timer(float current_temp) {
        if (millis() - last_called < 2000) {
            return false;
        }
        last_called = millis();
        return update_call(current_temp);
    }
    bool is_running() {
        return running_mode != Mode::Off;
    }
    private:
    Settings* settings;
    RTC_DS1307* rtc;
    unsigned long last_called;
    Mode running_mode;
};

#endif
