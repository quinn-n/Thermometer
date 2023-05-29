
#include <Keypad.h>
#include <DHT.h>

#include "menu.h"
#include "temp_mgr.h"

#define LCD_COLS 16
#define LCD_ROWS 2

#define DHT_PIN 2
#define DHT_TYPE DHT22

char KEYMAP[4][4] = {
    {'7', '8', '9', 'U'},
    {'4', '5', '6', 'D'},
    {'1', '2', '3', 'M'},
    {'.', '0', 'B', 'K'}
};

LiquidCrystal_I2C display = LiquidCrystal_I2C(0x27, LCD_COLS, LCD_ROWS);

Keypad keypad = Keypad(
    makeKeymap(KEYMAP),
    (byte[]) {9, 8, 7, 6},
    (byte[]) {13, 11, 12, 10},
    (byte) 4,
    (byte) 4
);

DHT dht(DHT_PIN, DHT_TYPE);

RTC_DS1307 rtc = RTC_DS1307();

Settings settings;

TempMgr temp_mgr = TempMgr(&settings, &rtc);

Menu menu = Menu(LCD_COLS, LCD_ROWS, &display, &keypad, &settings, &rtc, &temp_mgr);

bool update_display;

float old_temp;
DateTime old_time;

void setup() {
    Serial.begin(9600);
    Serial.print(F("Got sizeof TempSetting: "));
    Serial.println(String(sizeof(TempSetting)));
    Serial.print(F("Got "));
    Serial.print(String(MAX_CMPLX_TEMPS));
    Serial.println(F(" max temp settings"));
    display.init();
    display.backlight();
    display.print(F("Starting setup..."));
    dht.begin();
    display.setCursor(0, 1);
    display.print(F("dht"));
    rtc.begin();
    display.print(F(", rtc"));
    settings.begin();
    display.print(F(", settings"));
    display.clear();
    display.print(F("Finshed setup"));
}

void loop() {
    update_display = false;
    // Process keypresses
    /*
    keypad.getKeys();
    // TODO: Look into making menu run off events
    // keypad.addEventListener()
    */
    char key = keypad.getKey();
    // TODO: Make backlight flash when a key is pressed
    if (key) {
        Serial.print(F("loop() got key: "));
        Serial.println(key);
    }
    // TODO: Flash target temperature when it's being changed (while it differs from what's stored on eeprom?)
    if (key == 'U') {
        float new_temp = settings.simple_temp_setting.target_temp();
        new_temp += .5;
        settings.simple_temp_setting.target_temp(new_temp);
        update_display = true;
    }
    else if (key == 'D') {
        float new_temp = settings.simple_temp_setting.target_temp();
        new_temp -= .5;
        settings.simple_temp_setting.target_temp(new_temp);
        update_display = true;
    }
    else if (key == 'K') {
        settings.save_settings();
    }
    else if (key == 'M') {
        menu.run_menu();
        update_display = true;
    }

    float current_temp = dht.readTemperature();
    if (temp_mgr.update_call_timer(current_temp)) {
        update_display = true;
    }
    if (current_temp != old_temp) {
        old_temp = current_temp;
        update_display = true;
    }
    DateTime now = rtc.now();
    if (now.minute() != old_time.minute()) {
        old_time = now;
        update_display = true;
    }
    if (update_display) {
        menu.print_standby(current_temp);
    }
}
