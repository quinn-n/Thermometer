
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
        arx::pair<long, long> time_pair = user_query_time(F("Enter time:"));
        uint8_t hour = time_pair.first;
        uint8_t minute = time_pair.second;
        // Year, month and day are currently placeholders
        DateTime dt(2022, 18, 7, hour, minute);
        rtc->adjust(dt);
    }

    // Sets the unit (Celsius or Farenheit)
    void menu_select_units() {
    }

    // Adds a target temperature
    void menu_add_temp_setting() {
        arx::pair<long, long> time_pair = user_query_time(F("Enter time:"));
        if (time_pair.first == -1) {
            return;
        }
        long hour = time_pair.first;
        long minute = time_pair.second;

        String temp_str = user_query_temperature(F("Enter temp.:"));
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

        arx::pair<long, long> time_pair = user_query_time(F("Enter time:"));
        if (time_pair.first == -1) {
            return;
        }
        long hour = time_pair.first;
        long minute = time_pair.second;

        String temp_str = user_query_temperature(F("Eneter temp.:"));
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

    // Queries the user for a temperature
    String user_query_temperature(const String& query) {
        display->blink_on();
        String user_input = F("");

        while (true) {
            user_query_temperature_update_display(query, user_input);
            char key = keypad->waitForKey();
            if (key == 'B') {
                if (user_input.length() > 0) {
                    user_input = user_input.substring(0, user_input.length() - 1);
                } else {
                    break;
                }
            } else if (key == 'K') {
                break;
            } else {
                user_input += key;
            }
        }

        display->blink_off();
        return user_input;
    }

    // Updates the display when querying the user for a temperature input
    void user_query_temperature_update_display(const String& query, const String& user_input) {
        // Print query
        display->clear();
        display->setCursor(lcd_cols / 2 - query.length() / 2, 0);
        display->print(query);

        // Print current user input
        display->setCursor(lcd_cols / 2 - (user_input.length() + 1) / 2, 1);
        display->print(user_input);
        display->print('C');

        // Set cursor location for blinking
        display->setCursor(lcd_cols / 2 + user_input.length() / 2, 1);
    }

    // Queries the user for a time input. Returns <-1, -1> if the user backs out of the query.
    arx::pair<long, long> user_query_time(const String& query) {
        display->blink_on();
        String user_input = F("");

        while (true) {
            user_query_time_update_display(query, user_input);
            char key = keypad->waitForKey();
            if (key == 'B') {
                if (user_input.length() > 0) {
                    user_input = user_input.substring(0, user_input.length() - 1);
                } else {
                    break;
                }
            } else if (key == 'K') {
                break;
            } else {
                if (user_query_time_is_valid_input(user_input + key)) {
                    user_input += key;
                }
            }
        }

        display->blink_off();
        arx::pair<long, long> out_pair;
        if (user_input == "") {
            out_pair.first = -1;
            out_pair.second = -1;
        } else {
            String hour_str = user_input.substring(0, 2);
            // Add extra '0' onto `minute_str` if the user only input 1 character for the minute
            String minute_str = user_input.substring(2, 4);
            if (minute_str.length() < 2) {
                minute_str += ' ';
            }
            out_pair.first = hour_str.toInt();
            out_pair.second = minute_str.toInt();
        }
        return out_pair;
    }

    bool user_query_time_is_valid_input(const String& user_input) {
        if (user_input.length() > 4) {
            return false;
        }
        for (size_t i = 0; i < user_input.length(); i++) {
            char c = user_input[i];
            switch (i) {
                case 0:
                    if (c < '0' || c > '2') {
                        return false;
                    }
                    break;

                case 1:
                    if (user_input[0] < '2') {
                        if (c < '0' || c > '9') {
                            return false;
                        }
                    } else {
                        if (c < '0' || c > '3') {
                            return false;
                        }
                    }
                    break;
            
                case 2:
                    if (c < '0' || c > '5') {
                        return false;
                    }
                    break;
                
                case 3:
                    if (c < '0' || c > '9') {
                        return false;
                    }
                    break;

                default:
                    return false;
                    break;
                }
        }
        return true;
    }

    // Updates the display when querying the user for a time input
    void user_query_time_update_display(const String& query, String user_input) {
        // Print query
        display->clear();
        display->setCursor(lcd_cols / 2 - query.length() / 2, 0);
        display->print(query);

        size_t cursor_offset = user_input.length();
        // Add 1 to cursor offset for ':'
        if (cursor_offset >= 2) {
            cursor_offset++;
        }
        // Pad user input
        while (user_input.length() < 4) {
            user_input += '_';
        }
        String hour_str = user_input.substring(0, 2);
        String minute_str = user_input.substring(2, 4);
        display->setCursor(lcd_cols / 2 - strlen("00:00") / 2, 1);
        display->print(hour_str);
        display->print(':');
        display->print(minute_str);

        // Set cursor location for blinking
        display->setCursor(lcd_cols / 2 - strlen("00:00") / 2 + cursor_offset, 1);
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
