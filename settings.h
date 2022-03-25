
#ifndef SETTINGS_H
#define SETTINGS_H

#include <EEPROMWearLevel.h>

/*
EEPROM section layout
0 - Mode setting (Off / Cool / Heat / Fan)
1 - Complex / Simple Temperature setting
2 - Simple temperature setting
3 - Number of complex temperatures
4 - 63 - Complex temperature settings
*/

// EEPROMwl config
#define N_INDEXES 64

// EEPROM layout
#define MODE_IDX 0
#define TEMP_MODE 1
#define SIMPLE_TEMP_IDX 2
#define N_CMPLX_TEMPS_IDX 3
#define CMPLX_START_IDX 4

// number of indexes - number of reserved indexes
#define MAX_CMPLX_TEMPS N_INDEXES - 5

// setting constants
#define MODE_OFF 0
#define MODE_HEAT 1
#define MODE_COOL 2
#define MODE_FAN 3

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

    private:
    byte _target_temp;
    int _start_time;

    // NOTE: First 7 bits are the main temperature
    // The MSB is the half degree
    float decompress_target_temp(byte temp) {
        float n = temp & 0b11111110;
        if (temp & 0b00000001) {
            n += .5;
        }
        return n;
    }
    byte compress_target_temp(float temp) {
        byte b = floor(temp);
        if (fmod(temp, .5) == .5) {
            b |= 0b00000001;
        }
        return b;
    }
};

class Settings {
    public:
    byte mode;
    byte temp_mode;
    byte simple_temp_setting;

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
        TODO: Make this only run once (?)

        maybe it doesn't matter if EEPROMwl is initialized multiple
        times if it's done the same way?
        */
        EEPROMwl.begin(0, N_INDEXES);
        // Read settings from EEPROM
        mode = EEPROMwl.read(MODE_IDX);
        temp_mode = EEPROMwl.read(TEMP_MODE);
        simple_temp_setting = EEPROMwl.read(SIMPLE_TEMP_IDX);
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
    TempSetting* get_current_setting() {
        TempSetting* most_recent_setting = &_temp_settings[0];
    }

    // Private attribute access
    int8_t temp_settings(TempSetting** ts) {
        ts = &_temp_settings;
        return n_temp_settings;
    }

    // Writes settings to EEPROM
    void write_settings() {
        EEPROMwl.update(MODE_IDX, mode);
        EEPROMwl.update(TEMP_MODE, temp_mode);
        EEPROMwl.update(SIMPLE_TEMP_IDX, simple_temp_setting);
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
