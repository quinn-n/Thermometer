
#ifndef MENU_H
#define MENU_H

#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <RTClib.h>

#include "settings.h"
#include "temp_mgr.h"


const char* SUB_MENUS[7] = {
    "MODE",
    "CTRL MODE",
    "TIME",
    "UNITS",
    "ADD TGT TEMP",
    "DEL TGT TEMP",
    "EDIT TGT TEMP"
};

#define N_SUB_MENUS 7

class Menu {
    public:
    Settings* settings;
    Menu(int lcd_cols, int lcd_rows, LiquidCrystal_I2C* display, Keypad* keypad, Settings* settings, RTC_DS1307* rtc, TempMgr* temp_mgr) {
        this->display = display;
        this->lcd_cols = lcd_cols;
        this->lcd_rows = lcd_rows;

        this->keypad = keypad;

        this->rtc = rtc;
        this->settings = settings;
        this->temp_mgr = temp_mgr;
    }

    // Prints standby menu
    void print_standby(float current_temp) {
        display->clear();
        DateTime now = rtc->now();
        // Print current temperature
        // TODO: Make this flash when climate control is running
        String tempstr = String(current_temp);
        // TODO: Add custom 'degrees' symbol with `display->createChar`
        display->setCursor(4, 0);
        display->print(tempstr);

        // TODO: Add custom 'degrees' symbol with `display->createChar`
        // display->write((byte) 0);
        display->print('C');
        // Print asterisk if running
        if (temp_mgr->is_running()) {
            display->print('*');
        }

        // Print current mode
        Mode mode = settings->mode;
        if (mode == Mode::Off) {
            display->setCursor(lcd_cols - 5, 0);
            display->print("OFF");
        } else if (mode == Mode::Fan) {
            display->setCursor(lcd_cols - 5, 0);
            display->print("FAN");
        } else if (mode == Mode::Heat) {
            display->setCursor(lcd_cols - 4, 0);
            display->print("HEAT");
        } else if (mode == Mode::Cool) {
            display->setCursor(lcd_cols - 4, 0);
            display->print("COOL");
        } else if (mode == Mode::Auto) {
            display->setCursor(lcd_cols - 4, 0);
            display->print("AUTO");
        }

        // Print target temp
        display->setCursor(0, 1);
        display->print("TGT");
        tempstr = String(settings->get_current_setting(&now)->target_temp());
        // NOTE: The -1 might have to change depending on how the degrees symbol changes the positioning
        //display->setCursor(lcd_cols / 2 - tempstr.length() / 2 - 1, 1);
        display->setCursor(4, 1);
        display->print(tempstr);
        // TODO: Add custom 'degrees' symbol with `display->createChar`
        // display->write((byte) 0);
        display->print('C');

        //Print time
        if (rtc->isrunning()) {
            // Add leading 0s
            String hour_str = String(now.hour());
            while (hour_str.length() < 2) {
                hour_str = '0' + hour_str;
            }
            String minute_str = String(now.minute());
            while (minute_str.length() < 2) {
                minute_str = '0' + minute_str;
            }
            String time_str = hour_str + ':' + minute_str;
            display->setCursor(lcd_cols - time_str.length(), 1);
            display->print(time_str);
        } else {
            display->setCursor(lcd_cols - strlen("NORTC") - 1, 1);
            display->print("NORTC");
        }
    }

    void run_menu() {
        int8_t submenu = select_submenu(SUB_MENUS, N_SUB_MENUS);
        // Back
        if (submenu == -1) {
            return;
        }
        if (submenu == 0) {
            menu_set_mode();
        } else if (submenu == 1) {
            menu_set_control_mode();
        }

    }

    void show_error(const String& msg) {
        display->clear();
        display->setCursor(lcd_cols / 2 - strlen("ERROR") / 2, 0);
        display->print("ERROR");
        display->setCursor(lcd_cols / 2 - msg.length() / 2, 1);
        display->print(msg);
        display->flush();
        delay(2000);
        display->clear();
    }

    private:
    TempMgr* temp_mgr;
    LiquidCrystal_I2C* display;
    int lcd_cols;
    int lcd_rows;

    RTC_DS1307* rtc;
    Keypad* keypad;

    void menu_set_mode() {
        const char* smenus[5] = {
            "OFF",
            "HEAT",
            "COOL",
            "FAN",
            "AUTO"
        };
        int8_t mode_int = select_submenu(smenus, 5);
        if (mode_int == -1) {
            return;
        }
        settings->mode = (Mode) mode_int;
        Serial.println("Set mode to " + String(smenus[settings->mode]));
    }

    void menu_set_control_mode() {
        // NOTE: These menu entries have to match up with the `ControlMode` enum
        const char* smenus[2] = {
            "SIMPLE",
            "COMPLEX"
        };
        ControlMode control_mode = (ControlMode) select_submenu(smenus, 2);
        if (control_mode == -1) {
            return;
        }
        settings->control_mode = control_mode;
        Serial.println("Set temp control mode to " + String(settings->control_mode));
        // TODO: Save new mode to EEPROM
    }

    void menu_set_time() {
        String time_str = input();
        int split_idx = time_str.indexOf('.');
        if (split_idx == -1) {
            show_error("Invalid Time");
        }
        uint8_t hours = time_str.substring(0, split_idx).toInt();
    }

    int8_t select_submenu(const char** submenus, int n_menus) {
        int8_t menu_idx = 0;
        print_submenus(menu_idx, submenus, n_menus);
        display->blink_on();
        display->setCursor(0, 0);
        while (true) {
            char key = keypad->waitForKey();
            Serial.println("menus got key " + String(key));
            if (key >= '0' && key <= '9') {
                menu_idx = key - '0' - 1;
            }
            if (key == 'U') {
                Serial.println("Decrementing menu_idx");
                menu_idx--;
            }
            else if (key == 'D') {
                Serial.println("Incrementing menu_idx");
                menu_idx++;
            } else if (key == 'K') {
                display->blink_off();
                return menu_idx;
            } else if (key == 'B') {
                return -1;
            }
            menu_idx = wrap(0, n_menus, menu_idx);
            Serial.println("Printing submenu at idx " + String(menu_idx));
            print_submenus(menu_idx, submenus, n_menus);
            display->setCursor(0, 0);
            display->blink_on();
        }
    }

    String input() {
        String outstr = "";
        while (true) {
            char key = keypad->waitForKey();
            if (key == 'B') {
                if (outstr.length() > 0) {
                    outstr = outstr.substring(0, outstr.length() - 1);
                } else {
                    return "";
                }
            } else if (key == 'K') {
                return outstr;
            } else {
                outstr += key;
            }
        }
    }

    void print_submenus(uint8_t scroll_pos, const char** menu_options, uint8_t n_options) {
        display->clear();
        for (int row = 0; row < lcd_rows; row++) {
            display->setCursor(0, row);
            uint8_t option_n = wrap(0, n_options, row + scroll_pos);
            display->print(String(option_n + 1) + ". " + menu_options[option_n]);
        }
        display->flush();
    }

    // Wraps a number between s and e
    int wrap(int s, int e, int n) {
        if (n >= e) {
            return s + n % (e - s);
        }
        else if (n < s) {
            return e + n % (e - s);
        }
        else {
            return n;
        }
    }
};

#endif
