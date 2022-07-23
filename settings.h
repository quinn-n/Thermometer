
#ifndef SETTINGS_H
#define SETTINGS_H

#include <ArxContainer.h>
#include <EEPROMWearLevel.h>
#include <RTClib.h>

/*
EEPROM section layout
0 - Mode setting (Off / Cool / Heat / Fan)
1 - Complex/Simple temperature mode setting
2 - Simple temperature setting
3 - Number of complex temperatures
4 - 20 - Complex temperature settings
*/

// EEPROMwl config
#define N_INDEXES 21

// EEPROM layout
#define MODE_IDX 0
#define CONTROL_MODE_IDX 1
#define SIMPLE_TEMP_IDX 2
#define N_CMPLX_TEMPS_IDX 3
#define CMPLX_START_IDX 4

// number of indexes - total size of reserved indexes
#define MAX_CMPLX_TEMPS N_INDEXES - 5

// setting constants
enum Mode {
    Off = 0,
    Heat = 1,
    Cool = 2,
    Fan = 3,
    Auto = 4
};

enum ControlMode {
    Simple = 0,
    Complex = 1,
};

// There should only ever be one Settings object at a time
bool _settings_lock = false;

class TempSetting {
    public:
    TempSetting() {
        _target_temp = 0;
        _start_time = -1;
    }
    // t_temp - target temp in celsius, s_time - start time in seconds
    TempSetting(float t_temp, long s_time) {
        target_temp(t_temp);
        start_time(s_time);
    }
    TempSetting(float t_temp, DateTime& time) {
        target_temp(t_temp);
        long s_time = time.second() + (time.minute() + time.hour() * 60) * 60;
        start_time(s_time);
    }
    TempSetting(float t_temp, long hour, long minute) {
        long s_time = 60 * (minute + hour * 60);
        target_temp(t_temp);
        start_time(s_time);
    }
    TempSetting(TempSetting& other) {
        _target_temp = other._target_temp;
        _start_time = other._start_time;
    }

    float target_temp() const {
        return decompress_target_temp(_target_temp);
    }
    void target_temp(float new_temp) {
        _target_temp = compress_target_temp(new_temp);
    }
    long start_time() const {
        return _start_time;
    }
    void start_time(long new_time) {
        _start_time = new_time;
    }

    // Returns a human-readable string representation
    const String to_string() const {
        long m = start_time() / 60;
        long hour = floor(m / 60);
        long minute = m - hour * 60;
        String hour_str = String(hour);
        String minute_str = String(minute);
        // Pad hours and minutes
        while (hour_str.length() < 2) {
            hour_str = '0' + hour_str;
        }
        while (minute_str.length() < 2) {
            minute_str = '0' + minute_str;
        }
        return hour_str + ':' + minute_str + ": " + String(target_temp());
    }

    bool operator>(const TempSetting& other) {
        return _start_time > other._start_time;
    }

    private:
    byte _target_temp;
    long _start_time;

    static float decompress_target_temp(byte temp) {
        float n = (float) temp;
        n /= 2;
        return n;
    }
    static byte compress_target_temp(float temp) {
        byte b = round(temp * 2);
        return b;
    }
};

class Settings {
    public:
    Mode mode;
    ControlMode control_mode;
    TempSetting simple_temp_setting;
    arx::vector<TempSetting> temp_settings;

    Settings() {
        _settings_lock = true;
    }
    ~Settings() {
        _settings_lock = false;
    }
    void begin() {
        /*
        NOTE: Settings have to be initialized before they can be used anywhere else
        TODO: Make this only run once (?)

        maybe it doesn't matter if EEPROMwl is initialized multiple
        times if it's done the same way?
        */
        EEPROMwl.begin(2, N_INDEXES);
        // Read settings from EEPROM
        EEPROMwl.get(MODE_IDX, mode);
        EEPROMwl.get(CONTROL_MODE_IDX, control_mode);
        EEPROMwl.get(SIMPLE_TEMP_IDX, simple_temp_setting);
        size_t n_temp_settings = 0;
        EEPROMwl.get(N_CMPLX_TEMPS_IDX, n_temp_settings);
        temp_settings.reserve(MAX_CMPLX_TEMPS);
        for (size_t offset = 0; offset < n_temp_settings; offset++) {
            TempSetting ts;
            EEPROMwl.get(CMPLX_START_IDX + offset, ts);
            temp_settings.push_back(ts);
        }
    }

    // Saves settings to EEPROM
    void save_settings() {
        Serial.println(F("Saving settings to EEPROM... "));
        EEPROMwl.put(MODE_IDX, mode);
        EEPROMwl.put(CONTROL_MODE_IDX, control_mode);
        EEPROMwl.put(SIMPLE_TEMP_IDX, simple_temp_setting);

        EEPROMwl.put(N_CMPLX_TEMPS_IDX, temp_settings.size());
        for (int offset = 0; offset < temp_settings.size(); offset++) {
            EEPROMwl.put(CMPLX_START_IDX + offset, temp_settings[offset]);
        }
        Serial.println(F("Done!"));
    }

    /*
    Returns the TempSetting for the current time
    (or the simple setting if the control mode is simple OR `time` is NULL)
    */
    const TempSetting* get_current_setting(DateTime* time) {
        if (control_mode == ControlMode::Simple || time == NULL) {
            return &simple_temp_setting;
        }
        long current_second = (long) time->second() + 60 * ((long) time->minute() + (long) time->hour() * 60);
        // Search all the temp settings for the current day
        for (int i = temp_settings.size() - 1; i >= 0; i--) {
            const TempSetting& ts = temp_settings[i];
            if (current_second >= ts.start_time()) {
                return &ts;
            }
        }
        // Return the temp setting for the end of yesterday if no settings match
        return &temp_settings[temp_settings.size() - 1];
    }

    /*
    Add a temp setting
    Returns a 0 on success and 1 on failure
    */
    int add_temp_setting(const TempSetting& ts) {
        if (temp_settings.size() == MAX_CMPLX_TEMPS) {
            Serial.println(F("Failed to add new temp setting: temp_settings hit size limit"));
            return 1;
        }
        // Special case for first temp setting
        if (!temp_settings.size()) {
            temp_settings.push_back(ts);
            return 0;
        }
        // Insert temp setting into sorted `temp_settings`
        temp_settings.push_back(ts);
        // NOTE: The temp_settings don't seem to actually be getting sorted properly(?)
        sort_temp_settings();
        Serial.print(F("Added temp setting "));
        Serial.println(ts.to_string());
        return 0;
    }

    int add_temp_setting(float temp, uint8_t hour, uint8_t minute) {
        TempSetting ts(temp, hour, minute);
        return add_temp_setting(ts);
    }

    // Delete a temp setting
    void delete_temp_setting(size_t idx) {
        const auto& iter = temp_settings.begin() + idx;
        Serial.print(F("Deleting temp setting "));
        Serial.print(temp_settings[iter.index()].to_string());
        Serial.print(F(" at "));
        Serial.println(String(iter.index()));
        temp_settings.erase(iter);
    }

    private:
    void sort_temp_settings() {
        qsort(
            temp_settings.data(),
            temp_settings.size(),
            sizeof(TempSetting),
            _compare_temp_settings
        );
    }
    static int _compare_temp_settings(const void* a, const void* b) {
        const TempSetting* ts_a = (const TempSetting*) a;
        const TempSetting* ts_b = (const TempSetting*) b;
        if (ts_a->start_time() < ts_b->start_time()) {
            return -1;
        } else if (ts_a->start_time() == ts_b->start_time()) {
            return 0;
        } else {
            return 1;
        }
    }
};

#endif
