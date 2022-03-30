
#ifndef MENU_H
#define MENU_H

#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <RTClib.h>

#include "settings.h"
#include "temp_mgr.h"

#define CONTROL_MODE_SIMPLE 0
#define CONTROL_MODE_TIME 1

#define MODE_OFF 0
#define MODE_FAN 1
#define MODE_HEAT 2
#define MODE_COOL 3
#define MODE_AUTO 4

const char* SUB_MENUS[6] = {
    "CTRL MODE",
    "TIME",
    "UNITS",
    "ADD TGT TEMP",
    "DEL TGT TEMP",
    "EDIT TGT TEMP"
};

#define N_SUB_MENUS 6

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
        String tempstr = String(current_temp);
        // TODO: Add custom 'degrees' symbol with `display->createChar`
        /*
        display->setCursor(lcd_cols / 2 - tempstr.length() / 2, 0);
        */
        display->setCursor(4, 0);
        display->print(tempstr);

        // TODO: Add custom 'degrees' symbol with `display->createChar`
        // display->write((byte) 0);
        display->print('C');

        // Print current mode
        if (mode == MODE_OFF) {
            display->setCursor(lcd_cols - 5, 0);
            display->print("OFF");
        } else if (mode == MODE_FAN) {
            display->setCursor(lcd_cols - 5, 0);
            display->print("FAN");
        } else if (mode == MODE_HEAT) {
            display->setCursor(lcd_cols - 6, 0);
            display->print("HEAT");
        } else if (mode == MODE_COOL) {
            display->setCursor(lcd_cols - 6, 0);
            display->print("COOL");
        } else if (mode == MODE_AUTO) {
            display->setCursor(lcd_cols - 6, 0);
            display->print("AUTO");
        }
        // Print asterisk if running
        if (temp_mgr->is_running()) {
            display->setCursor(lcd_cols, 0);
            display->print('*');
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
            menu_control_mode();
        } else if (submenu == 1) {

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

    uint8_t mode;

    void menu_control_mode() {
        const char* smenus[2] = {
            "SIMPLE",
            "COMPLX"
        };
        int8_t new_mode = select_submenu(smenus, 2);
        if (new_mode == -1) {
            return;
        }
        settings->temp_mode = new_mode;
        Serial.println("Set temp mode to " + String(settings->temp_mode));
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
