
#ifndef MENU_H
#define MENU_H

#include <ArxContainer.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <RTClib.h>

#include "settings.h"
#include "temp_mgr.h"


const char* SUB_MENUS[7] = {
    "MODE",
    "CTRL MODE",
    "TIME",
    "SELECT UNITS",
    "ADD TEMP SET",
    "DEL TEMP SET",
    "EDIT TEMP SET"
};

#define N_SUB_MENUS 7

arx::map<char, char> SHIFT_KEYS {
    {'.', ':'}
};

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
            display->print(F("OFF"));
        } else if (mode == Mode::Fan) {
            display->setCursor(lcd_cols - 5, 0);
            display->print(F("FAN"));
        } else if (mode == Mode::Heat) {
            display->setCursor(lcd_cols - 4, 0);
            display->print(F("HEAT"));
        } else if (mode == Mode::Cool) {
            display->setCursor(lcd_cols - 4, 0);
            display->print(F("COOL"));
        } else if (mode == Mode::Auto) {
            display->setCursor(lcd_cols - 4, 0);
            display->print(F("AUTO"));
        }

        // Print target temp
        display->setCursor(0, 1);
        display->print(F("TGT"));
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
            display->print(F("NORTC"));
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
        } else if (submenu == 2) {
            menu_set_time();
        } else if (submenu == 3) {
            // NOTE: unimplemented
            menu_select_units();
        } else if (submenu == 4) {
            menu_add_temp_setting();
        } else if (submenu == 5) {
            menu_del_temp_setting();
        } else if (submenu == 6) {
            menu_edit_temp_setting();
        }
        // Write updated settings to EEPROM
        settings->save_settings();
    }

    void show_error(const String& msg) {
        display->clear();
        display->setCursor(lcd_cols / 2 - strlen("ERROR") / 2, 0);
        display->print(F("ERROR"));
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
    }

    // Sets the time for the RTC
    void menu_set_time() {
        String time_str = user_query("Enter time:", true);
        int split_idx = time_str.indexOf(':');
        if (split_idx == -1) {
            show_error(F("Invalid Time"));
            return;
        }
        uint8_t hour = time_str.substring(0, split_idx).toInt();
        uint8_t minute = time_str.substring(split_idx + 1, time_str.length()).toInt();
        // Year, month and day are currently placeholders
        DateTime dt(2022, 18, 7, hour, minute);
        rtc->adjust(dt);
    }

    // Sets the unit (Celsius or Farenheit)
    void menu_select_units() {
    }

    // Adds a target temperature
    void menu_add_temp_setting() {
        String time_str = user_query(F("Enter time:"), true);
        int split_idx = time_str.indexOf(':');
        if (split_idx == -1) {
            show_error(F("Invalid Time"));
            return;
        }
        long hour = time_str.substring(0, split_idx).toInt();
        long minute = time_str.substring(split_idx + 1, time_str.length()).toInt();

        String temp_str = user_query(F("Enter temp.:"), false);
        float temp = temp_str.toFloat();

        int status = settings->add_temp_setting(temp, hour, minute);
        if (status) {
            show_error(F("Max Temp Sets"));
        }
    }

    // Deletes a target temperature
    void menu_del_temp_setting() {

        int selection = user_select_temp_setting();
        if (selection == -1) {
            return;
        }
        //Confirmation dialog
        bool confirmation = user_confirm("Delete " + settings->temp_settings[selection].to_string() + '?');
        if (confirmation) {
            settings->delete_temp_setting(selection);
        }
    }

    // Edits a target temperature
    void menu_edit_temp_setting() {
        int selection = user_select_temp_setting();
        if (selection == -1) {
            return;
        }

        // TODO: Move this code block into its own function, along with `user_query_time` and `user_query_temperature`
        String time_str = user_query(F("Enter time:"), true);
        int split_idx = time_str.indexOf(':');
        if (split_idx == -1) {
            show_error(F("Invalid Time"));
            return;
        }
        long hour = time_str.substring(0, split_idx).toInt();
        long minute = time_str.substring(split_idx + 1, time_str.length()).toInt();

        String temp_str = user_query(F("Enter temp.:"), false);
        float temp = temp_str.toFloat();

        // Delete the old temp setting and create a new one
        settings->delete_temp_setting(selection);
        int status = settings->add_temp_setting(temp, hour, minute);
    }

    // Query the user to select a temp setting
    int user_select_temp_setting() {
        // Load tempsettings into submenus array
        const char** submenus = new const char*[settings->temp_settings.size()];
        if (settings->temp_settings.size() == 0) {
            show_error(F("No temp settings"));
            return -1;
        }
        for (size_t i = 0; i < settings->temp_settings.size(); i++) {
            const TempSetting& ts = settings->temp_settings[i];
            const String setting_str = ts.to_string();
            submenus[i] = new char[setting_str.length()];
            strcpy((char*) submenus[i], setting_str.c_str());
        }
        // Get selection from user
        int8_t selection = select_submenu(submenus, settings->temp_settings.size());
        return selection;
    }

    // Queries the user with a list of submenus
    int8_t select_submenu(const char** submenus, int n_menus) {
        int8_t menu_idx = 0;
        print_submenus(menu_idx, submenus, n_menus);
        display->blink_on();
        display->setCursor(0, 0);
        while (true) {
            char key = keypad->waitForKey();
            if (key >= '0' && key <= '9') {
                menu_idx = key - '0' - 1;
            }
            if (key == 'U') {
                menu_idx--;
            }
            else if (key == 'D') {
                menu_idx++;
            } else if (key == 'K') {
                display->blink_off();
                return menu_idx;
            } else if (key == 'B') {
                return -1;
            }
            menu_idx = wrap(0, n_menus, menu_idx);
            print_submenus(menu_idx, submenus, n_menus);
            display->setCursor(0, 0);
            display->blink_on();
        }
    }

    // Queries the user for a line of input
    String user_query(const String& query, bool shift_keys) {
        String user_input = "";
        while (true) {
            user_query_update_display(query, user_input);
            char key = keypad->waitForKey();
            if (shift_keys) {
                key = shift_key(key);
            }
            if (key == 'B') {
                if (user_input.length() > 0) {
                    user_input = user_input.substring(0, user_input.length() - 1);
                } else {
                    return "";
                }
            } else if (key == 'K') {
                return user_input;
            } else {
                user_input += key;
            }
        }
    }

    // Updates the display when querying the user
    void user_query_update_display(const String& query, String& user_input) {
        // Print query
        display->clear();
        display->setCursor(lcd_cols / 2 - query.length() / 2, 0);
        display->print(query);

        // Print current user input
        display->setCursor(lcd_cols / 2 - user_input.length() / 2, 1);
        display->print(user_input);
    }

    // Returns the shifted version of a key
    char shift_key(char k) {
        if (SHIFT_KEYS.find(k) == SHIFT_KEYS.end()) {
            return k;
        } else {
            return SHIFT_KEYS[k];
        }
    }

    // Asks the user to confirm a query
    bool user_confirm(const String& query) {
        display->clear();
        display->setCursor(lcd_cols / 2 - query.length() / 2, 0);
        display->print(query);
        const String confirm_dialog = F("K - OK, B - Back");
        display->setCursor(lcd_cols / 2 - confirm_dialog.length() / 2, 1);
        display->print(confirm_dialog);
        char key = 0;
        while (key != 'K' && key != 'B') {
            key = keypad->waitForKey();
        }
        if (key == 'K') {
            return true;
        } else {
            return false;
        }
    }

    void print_submenus(uint8_t scroll_pos, const char** menu_options, uint8_t n_options) {
        display->clear();
        for (int row = 0; row < lcd_rows; row++) {
            display->setCursor(0, row);
            uint8_t option_n = wrap(0, n_options, row + scroll_pos);
            display->print(String(option_n + 1) + ". " + menu_options[option_n]);
        }
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
