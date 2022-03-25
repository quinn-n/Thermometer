
#ifndef MENU_H
#define MENU_H

#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <RTClib.h>

#include "settings.h"

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

#define N_SUB_MENUS sizeof(SUB_MENUS) / 16

class Menu {
    public:
    Settings* settings;
    Menu(int lcd_cols, int lcd_rows, Keypad* keypad) {
        display = new LiquidCrystal_I2C(0x27, lcd_cols, lcd_rows);
        this->lcd_cols = lcd_cols;
        this->lcd_rows = lcd_rows;

        this->keypad = keypad;

        this->rtc = new RTC_DS1307();
        settings = new Settings();
        settings->begin();
        // TODO: Load target temp from eeprom
        tgt_temp = 21.5;
    }

    // Prints standby menu
    void print_standby(float current_temp) {
        // Print current temperature
        String tempstr = String(current_temp);
        display->setCursor(lcd_cols / 2 - tempstr.length() / 2, 0);
        display->print(tempstr);
        // Print current mode
        display->print(' ');
        if (mode == MODE_OFF) {
            display->print("OFF");
        } else if (mode == MODE_FAN) {
            display->print("FAN");
        } else if (mode == MODE_HEAT) {
            display->print("HEAT");
        } else if (mode == MODE_COOL) {
            display->print("COOL");
        } else if (mode == MODE_AUTO) {
            display->print("AUTO");
        }
        // TODO: Print asterisk if running

        // Print target temp
        display->setCursor(0, 1);
        display->print("TGT");
        tempstr = tgt_temp;
        display->setCursor(lcd_cols / 2 - tempstr.length() / 2, 1);
        display->print(tempstr);

        //Print time
        if (rtc->isrunning()) {
            DateTime now = rtc->now();
            String time_str = String(now.hour()) + ':' + String(now.minute());
            display->setCursor(lcd_cols - time_str.length() - 1, 1);
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
    LiquidCrystal_I2C* display;
    int lcd_cols;
    int lcd_rows;

    RTC_DS1307* rtc;
    Keypad* keypad;

    double tgt_temp;
    int8_t control_mode;
    uint8_t mode;

    void menu_control_mode() {
        const char* smenus[2] = {
            "SIMPLE",
            "TIME"
        };
        int8_t new_mode = select_submenu(smenus, 2);
        if (new_mode == -1) {
            return;
        }
        control_mode = new_mode;
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
        print_submenus(menu_idx, submenus);
        while (true) {
            char key = keypad->waitForKey();
            if (key >= '0' && key <= '9') {
                menu_idx = key - '0';
            }
            if (key == 'U') {
                menu_idx++;
            }
            else if (key == 'D') {
                menu_idx--;
            } else if (key == 'K') {
                return menu_idx;
            } else if (key == 'B') {
                return -1;
            }
            if (menu_idx > n_menus) {
                menu_idx = n_menus;
            } else if (menu_idx < 0) {
                menu_idx = 0;
            }
            print_submenus(menu_idx, submenus);
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

    void print_submenus(uint8_t scroll_pos, const char** menu_options) {
        display->clear();
        for (int row = 0; row < lcd_rows; row++) {
            display->setCursor(0, row);
            display->print(String(row + scroll_pos + 1) + ". " + menu_options[row + scroll_pos]);
        }
        display->flush();
    }
};

#endif
