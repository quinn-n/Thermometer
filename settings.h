
#ifndef SETTINGS_H
#define SETTINGS_H

#include <EEPROMWearLevel.h>
#include <RTClib.h>

/*
EEPROM section layout
0 - Mode setting (Off / Cool / Heat / Fan)
1 - Complex/Simple temperature mode setting
2, 3 - Simple temperature setting
4 - Number of complex temperatures
5 - 63 - Complex temperature settings
*/

// EEPROMwl config
#define N_INDEXES 64

// EEPROM layout
#define MODE_IDX 0
#define CONTROL_MODE 1
#define SIMPLE_TEMP_IDX 2
#define N_CMPLX_TEMPS_IDX SIMPLE_TEMP_IDX + sizeof(TempSetting)
#define CMPLX_START_IDX N_CMPLX_TEMPS_IDX + 1

// number of indexes - total size of reserved indexes
#define MAX_CMPLX_TEMPS N_INDEXES - 6

// setting constants
enum Mode {
    Off = 0,
    Heat = 1,
    Cool = 2,
    Fan = 3,
    Auto = 4
};

#define MODE_SIMPLE 0
#define MODE_CMPLX 1

// There should only ever be one Settings object at a time
bool settings_lock = false;

class TempSetting {
    public:
    TempSetting() {
        _target_temp = 0;
        _start_time = -1;
    }
    TempSetting(float t_temp, int s_time) {
        this->_target_temp = t_temp;
        this->_start_time = s_time;
    }
    TempSetting(TempSetting& other) {
        _target_temp = other._target_temp;
        _start_time = other._start_time;
    }

    float target_temp() {
        return decompress_target_temp(_target_temp);
    }
    void target_temp(float new_temp) {
        _target_temp = compress_target_temp(new_temp);
    }
    int start_time() {
        return _start_time;
    }
    void start_time(int new_time) {
        _start_time = new_time;
    }

    // Returns a human-readable string representation
    String to_string() {
        return String(start_time()) + ": " + String(target_temp());
    }

    private:
    byte _target_temp;
    int _start_time;

    float decompress_target_temp(byte temp) {
        float n = (float) temp;
        n /= 2;
        return n;
    }
    byte compress_target_temp(float temp) {
        byte b = round(temp * 2);
        return b;
    }
};

class Settings {
    public:
    Mode mode;
    byte control_mode;
    TempSetting simple_temp_setting;

    Settings() {
        settings_lock = true;
    }
    // TODO: Consider making this function write temp_settings to EEPROM
    ~Settings() {
        delete _temp_settings;
        settings_lock = false;
    }
    void begin() {
        /*
        NOTE: Settings have to be initialized before they can be used anywhere else
        TODO: Make this only run once (?)

        maybe it doesn't matter if EEPROMwl is initialized multiple
        times if it's done the same way?
        */
        EEPROMwl.begin(0, N_INDEXES);
        // Read settings from EEPROM
        Mode mode;
        EEPROMwl.get(MODE_IDX, mode);
        control_mode = EEPROMwl.read(CONTROL_MODE);
        EEPROMwl.get(SIMPLE_TEMP_IDX, simple_temp_setting);
        n_temp_settings = EEPROMwl.read(N_CMPLX_TEMPS_IDX);
        _temp_settings = new TempSetting[MAX_CMPLX_TEMPS];
        for (int offset = 0; offset < n_temp_settings; offset++) {
            EEPROMwl.get(CMPLX_START_IDX + offset, _temp_settings[offset]);
        }
    }

    /*
    TODO: Make this return the current temp setting
    based on the time from the rtc

    NOTE: Don't forget to make it include the last setting from
    the previous day somehow
    */
    TempSetting* get_current_setting(DateTime* time) {
        if (control_mode == MODE_SIMPLE || time == NULL) {
            return &simple_temp_setting;
        }
        int current_second = 60 * (time->minute() + time->hour() * 60);
        // Search all the temp settings for the current day
        for (byte i = 0; i < n_temp_settings; i++) {
            if (_temp_settings[i].start_time() <= current_second) {
                return &_temp_settings[i];
            }
        }
        // Return the temp setting for the end of yesterday if no settings are found
        return &_temp_settings[n_temp_settings - 1];
    }

    // Private attribute access
    int8_t temp_settings(TempSetting** ts) {
        ts = &_temp_settings;
        return n_temp_settings;
    }

    // Writes settings to EEPROM
    void write_settings() {
        EEPROMwl.update(MODE_IDX, mode);
        EEPROMwl.update(CONTROL_MODE, control_mode);
        EEPROMwl.put(SIMPLE_TEMP_IDX, simple_temp_setting);
        EEPROMwl.put(N_CMPLX_TEMPS_IDX, n_temp_settings);
        for (int offset = 0; offset < n_temp_settings; offset++) {
            EEPROMwl.put(CMPLX_START_IDX, _temp_settings[offset]);
        }
    }
    private:
    int8_t n_temp_settings;
    TempSetting* _temp_settings;
};

#endif
