
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
    (byte[]) {6, 5, 4, 3},
    (byte[]) {7, 8, 9, 10},
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
    display.init();
    display.backlight();
    display.println("Starting setup...");
    display.flush();
    dht.begin();
    rtc.begin();
    settings.begin();
    display.clear();
    display.println("Finshed setup");
    display.flush();
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
        Serial.println("loop() got key: " + String((char) key));
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
    temp_mgr.update_call_timer(current_temp);
    /*
    TODO: Only update the display if it actually changes.
      - Class built on LiquidCrystal that includes a frame buffer + a hash of the last flashed frame?
        - BufferedLiquidCrystal_I2C or something

    Otherwise this makes the LCD flash every time it's called.
    */
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
    /*
    TODO: Get rid if this delay() and track the last time different methods were called
    */
}
