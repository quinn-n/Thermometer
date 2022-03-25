/*
 * Thermometer.ino
 * Sketch for a thermometer can change its temperature at preconfigured times
 * The basic idea behind this is you can crank your a/c during off-peak hours
 * and turn it back up during on-peak hours to save money.
 *
 * Written by Quinn Neufeld
 *
 */

#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <RTClib.h>

#include <Keypad.h>
#include <EEPROMWearLevel.h>

#define SERIAL_RATE 9600

// Display config
#define LCD_COLS 16
#define LCD_ROWS 2
#define FLASH_DURATION 4000

// Storage config
#define MAX_N_PEAKS 8
#define MAX_TEMPS 64 - MAX_N_PEAKS - 3 - 1

// Sensor config
#define DHT_TYPE DHT22
#define DHT_PIN 2

// Relay config
#define HEAT_PIN 3
#define COOL_PIN 4
#define FAN_PIN 5

// Peak constants
#define OFF_PEAK 0
#define MID_PEAK 1
#define ON_PEAK 2
#define UNKNOWN_PEAK 3
#define NO_RTC 4

// Mode constants
#define MODE_OFF 0
#define MODE_HEAT 1
#define MODE_COOL 2
#define MODE_FAN 3
#define MODE_AUTO 4

// Control constants
#define CONTROL_SIMPLE 0
#define CONTROL_TIME 1
#define CONTROL_PEAK 2

// Menu constants & progmem
const PROGMEM char menu_opts[7][16] = {
    "MODE",
    "TIME",
    "TEMP UNIT",
    "PEAK TIME",
    "ADD TEMP TGT",
    "DEL TEMP TGT",
    "EDIT TEMP TGT"
};
#define N_MENU_OPTS sizeof(menu_opts) / 16

typedef struct
{
    long tod;
    byte peak;
} peak;

struct peak_list
{
    peak_list *prev;
    peak p;
    peak_list *next;
};

typedef struct
{
    // In peak mode, time is the peak value. In time mode, time is the time of day (in seconds.)
    // If time is negative, the setting is for peak mode.
    long time;
    byte temp;
} temp_setting;

struct temp_list
{
    temp_list *prev;
    temp_setting ts;
    temp_list *next;
};

peak_list *peaks;
temp_list *temps;

// lcd global vars
bool refresh_lcd = true;
bool farenheit = false;
unsigned long flash_time = 0;
byte old_minute = 0;

// Keypad vars
byte colPins[4] = {3, 4, 5, 6};
byte rowPins[4] = {7, 8, 9, 10};
char keymap[4][4] = {
    {'7', '8', '9', 'U'},
    {'4', '5', '6', 'D'},
    {'1', '2', '3', 'M'},
    {'.', '0', 'B', 'K'}
};

// temperature control vars
float temp = 0;
float tgt_temp = 0;
byte mode = MODE_OFF;
bool running = false;

// peak vars
byte old_peak = UNKNOWN_PEAK;

LiquidCrystal_I2C lcd(0x27, LCD_COLS, LCD_ROWS);
DHT dht(DHT_PIN, DHT_TYPE);
RTC_DS1307 rtc;

Keypad keypad = Keypad(makeKeymap(keymap), rowPins, colPins, 4, 4);

void update_target_temp();

void read_settings();
void add_peak();
peak_list *get_last_peak();

void update_disp_standby();
void print_peak(byte);
void print_temp(float, bool);
void print_mode();
void print_running();
void print_tgt_temp(float, bool);
void print_time();

// Menu functions
void show_main_menu();

void scroll_menu(int);

float read_simp_temp(char);

char delay_get_key(size_t);

void update_relays();

byte get_peak();

byte temp_setting_ctrl(temp_setting &);

float expand_temp(byte);
byte compress_temp(float);

float convert_farenheit(float);
float round_to_half(float);

void setup()
{
    pinMode(HEAT_PIN, OUTPUT);
    pinMode(COOL_PIN, OUTPUT);
    pinMode(FAN_PIN, OUTPUT);

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Starting...");

    dht.begin();

    EEPROMwl.begin(0, 64);
    read_settings();

    lcd.clear();
    lcd.noBacklight();
}

void loop()
{
    unsigned long stime = millis();

    temp = dht.readTemperature();

    update_relays();

    // Update LCD if time changes
    if (rtc.isrunning())
    {
        DateTime now = rtc.now();
        if (now.minute() != old_minute)
        {
            refresh_lcd = true;
            old_minute = now.minute();
        }
    }

    // Set display backlight
    if (millis() - flash_time > FLASH_DURATION)
    {
        lcd.backlight();
    }
    else
    {
        lcd.noBacklight();
    }
    if (refresh_lcd)
    {
        update_disp_standby();
        refresh_lcd = false;
    }

    // delay(2000 - (millis() - stime));
    char k = delay_get_key(2000 - (millis() - stime));

    if (k != NO_KEY)
    {
        lcd.backlight();

        if (k == 'M')
        {
            show_main_menu();
        }

        if (EEPROMwl.read(0) == CONTROL_SIMPLE)
        {
            if ((k >= '0' && k <= '9') || k == 'U' || k == 'D' || k == '.')
            {
                float new_tgt = read_simp_temp(k);
            }
        }

        lcd.noBacklight();
    }
}

/*
 * Reads temperature and peak settings from EEPROM
 */
void read_settings()
{
    byte ctrl_type = EEPROMwl.read(0);
    mode = EEPROMwl.read(1);
    // Read peak times from EEPROM
    byte n_peaks = EEPROMwl.read(2);
    if (n_peaks)
    {
        peak_list *peak_cache = new peak_list;
        for (int i = 3; i < n_peaks + 3; i++)
        {
            peak p;
            EEPROMwl.get(i, p);
            // Add new peak to linked list
            peak_cache->p = p;
            peak_cache->next = new peak_list;
            peak_cache->next->prev = peak_cache;
            peak_cache = peak_cache->next;
        }
        // Last spot in list will be unused
        peaks = peak_cache->prev;
        delete peak_cache;
    }
    // Read temperature settings from EEPROM
    byte n_temps = EEPROMwl.read(MAX_N_PEAKS + 3);
    if (n_temps)
    {
        temp_list *temp_cache = new temp_list;
        for (int i = MAX_N_PEAKS + 4; i < n_temps + MAX_N_PEAKS + 4; i++)
        {
            temp_setting ts;
            EEPROMwl.get(i, ts);
            // Add new temp to temp_list
            temp_cache->ts = ts;
            temp_cache->next = new temp_list;
            temp_cache->next->prev = temp_cache;
            temp_cache = temp_cache->next;
        }
        // Last spot in list will be unused
        temps = temp_cache->prev;
        delete temp_cache;
    }
}

/*
 * Adds peak to linked list
 */
void add_peak(peak &p)
{
    // Special case for first peak
    if (!peaks)
    {
        peaks = new peak_list;
        peaks->p = p;
        return;
    }
    peak_list *lpeak = get_last_peak();
    lpeak->next = new peak_list;
    lpeak->next->prev = peaks;
    lpeak->next->p = p;
}

/*
 * Returns the last peak in the list
 */
peak_list *get_last_peak()
{
    peak_list *pl = peaks;
    while (pl->next)
    {
        pl = pl->next;
    }
    return pl;
}

/*
 * Function that updates the display in standby mode
 */
void update_disp_standby()
{
    lcd.clear();
    // Print peak
    print_peak(get_peak());

    // Print current temperature on lcd
    float temp = dht.readTemperature();
    print_temp(temp, farenheit);

    // Print out mode on LCD
    print_mode();

    // Print out running asterisk
    print_running();

    // Print out target tempreature
    print_tgt_temp(tgt_temp, farenheit);

    // Print out time
    print_time();
}

/*
 * Prints the given peak to the LCD
 */
void print_peak(byte p)
{
    lcd.setCursor(0, 0);
    switch (p)
    {
    case OFF_PEAK:
        lcd.print("OP");
        break;
    case MID_PEAK:
        lcd.print("MP");
        break;
    case ON_PEAK:
        lcd.print('P');
        break;
    case UNKNOWN_PEAK:
        lcd.print("UP");
        break;
    case NO_RTC:
        lcd.print("NRTC");
        break;
    };
}

/*
 * Prints the temperature on the LCD
 */
void print_temp(float t, bool f)
{
    if (f)
    {
        temp = round_to_half(convert_farenheit(temp));
    }
    String tempstr = String(temp);
    lcd.setCursor(LCD_COLS / 2 - tempstr.length() / 2, 0);
    lcd.print(tempstr);
    if (f)
    {
        lcd.print('F');
    }
    else
    {
        lcd.print('C');
    }
}

/*
 * Prints the mode to the LCD
 */
void print_mode()
{
    lcd.setCursor(LCD_COLS - 7, 0);
    switch (mode)
    {
    case MODE_HEAT:
        lcd.print("HEAT");
        break;
    case MODE_COOL:
        lcd.print("COOL");
        break;
    case MODE_FAN:
        lcd.print("FAN");
        break;
    case MODE_AUTO:
        lcd.print("AUTO");
        break;
    };
}

/*
 * Prints the running asterisk on the LCD
 */
void print_running()
{
    if (running)
    {
        lcd.setCursor(LCD_COLS - 1, 0);
        lcd.print('*');
    }
}

/*
 * Prints the target temperataure to the LCD
 */
void print_tgt_temp(float temp, bool f)
{
    lcd.setCursor(0, 1);
    lcd.print("TGT ");
    if (f)
    {
        temp = round_to_half(temp * 9 / 5. + 32);
    }
    lcd.print(temp);
    if (f)
    {
        lcd.print('F');
    }
    else
    {
        lcd.print('C');
    }
}

// Print the current time to the LCD
void print_time()
{
    lcd.setCursor(LCD_COLS - 7, 1);
    if (rtc.isrunning())
    {
        DateTime now = rtc.now();
        lcd.print(String(now.hour()) + ':' + String(now.minute()));
    }
    // Else print NORTC if the rtc is not initialized
    else
    {
        lcd.print("NORTC");
    }
}

/*
 * Opens the main menu
 */
void show_main_menu()
{
    int menu_pos = 0;
    scroll_menu(menu_pos);

    lcd.cursor();
    lcd.blink();

    while (true)
    {
        char k = keypad.waitForKey();
        if (k >= '0' && k <= '9')
        {
            menu_pos = k - '0';
            if (menu_pos > N_MENU_OPTS)
            {
                menu_pos = N_MENU_OPTS;
            }
            scroll_menu(menu_pos);
            continue;
        }
        if (k == 'U')
        {
            menu_pos++;
            if (menu_pos > N_MENU_OPTS)
            {
                menu_pos = 0;
            }
            scroll_menu(menu_pos);
            continue;
        }
        if (k == 'D')
        {
            menu_pos--;
            if (menu_pos < 0)
            {
                menu_pos = N_MENU_OPTS;
            }
            scroll_menu(menu_pos);
            continue;
        }
        if (k == 'M' || k == 'K')
        {
            // Open the selected submenu
        }
        if (k == 'B')
        {
            break;
        }
    }

    lcd.noCursor();
    lcd.noBlink();
}

/*
 * Scrolls menu to given position
 */

void scroll_menu(int pos)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    for (int i = pos; i < pos + LCD_ROWS; i++)
    {
        lcd.print(String(i + 1) + '.');
        size_t len = strlen_P(pgm_read_word(menu_opts[i]));
        for (size_t n = 0; n < len; n++)
        {
            lcd.print(pgm_read_word(menu_opts[i]));
        }
        lcd.println();
    }
    lcd.setCursor(0, 0);
}

/*
 * Reads a simple temperature input from the user
 */
float read_simp_temp(char k)
{
    lcd.cursor();
    lcd.blink();
    String cache = String(tgt_temp);
    while (k != 'K')
    {
        if (k == 'U')
        {
            float n = cache.toFloat() + .5;
            if (n > 99.5)
                n = 99.5;
            cache = String(n);
            lcd.setCursor(1, 7);
        }
        else if (k == 'D')
        {
            float n = cache.toFloat() - .5;
            if (n < 0)
                n = 0;
            cache = String(n);
            lcd.setCursor(1, 7);
        }
        print_tgt_temp(cache.toFloat(), farenheit);
    }
    // TODO: Let user manually input a temperature (will be weird with farenheit)
    lcd.noCursor();
    lcd.noBlink();
}

/*
 * Delays for a given number of ms.
 * If a key on the keypad is pressed, returns that key
 */
char delay_get_key(size_t timeout)
{
    size_t start = millis();
    while (millis() - start < timeout)
    {
        char k = keypad.getKey();
        if (k != NO_KEY)
        {
            return k;
        }
    }
    return NO_KEY;
}

/*
 * Updates relays based on the difference between the target temperature and the ambient air temperature, and the the mode
 */
void update_relays()
{
    // Set relays
    switch (mode)
    {
    case MODE_HEAT:
        if (temp < tgt_temp - .5)
        {
            if (!running)
            {
                flash_time = millis();
                running = true;
            }
            digitalWrite(HEAT_PIN, HIGH);
            digitalWrite(FAN_PIN, HIGH);
        }
        if (temp > tgt_temp + .5)
        {
            if (running)
            {
                flash_time = millis();
                running = false;
            }
            digitalWrite(HEAT_PIN, LOW);
            digitalWrite(FAN_PIN, LOW);
        }
        break;
    case MODE_COOL:
        if (temp > tgt_temp + .5)
        {
            if (!running)
            {
                flash_time = millis();
                running = true;
            }
            digitalWrite(COOL_PIN, HIGH);
            digitalWrite(FAN_PIN, HIGH);
        }
        if (temp < tgt_temp - .5)
        {
            if (running)
            {
                flash_time = millis();
                running = false;
            }
            digitalWrite(COOL_PIN, LOW);
            digitalWrite(FAN_PIN, LOW);
        }
        break;
    case MODE_FAN:
        digitalWrite(FAN_PIN, HIGH);
        break;
    case MODE_AUTO:
        // Heat on
        if (temp < tgt_temp - .5)
        {
            if (!running)
            {
                flash_time = millis();
                running = true;
                refresh_lcd = true;
            }
            digitalWrite(HEAT_PIN, HIGH);
            digitalWrite(FAN_PIN, HIGH);
        }
        // Heat off
        if (!digitalRead(COOL_PIN) && temp > tgt_temp)
        {
            if (running)
            {
                flash_time = millis();
                running = false;
                refresh_lcd = true;
            }
            digitalWrite(HEAT_PIN, LOW);
            digitalWrite(FAN_PIN, LOW);
        }
        // Cool on
        if (temp > tgt_temp + .5)
        {
            if (!running)
            {
                flash_time = millis();
                running = true;
                refresh_lcd = true;
            }
            digitalWrite(COOL_PIN, HIGH);
            digitalWrite(FAN_PIN, HIGH);
        }
        // Cool off
        if (!digitalRead(HEAT_PIN) && temp < tgt_temp)
        {
            if (running)
            {
                flash_time = millis();
                running = false;
                refresh_lcd = true;
            }
            digitalWrite(COOL_PIN, LOW);
            digitalWrite(FAN_PIN, LOW);
        }
        break;
    case MODE_OFF:
        digitalWrite(FAN_PIN, LOW);
        digitalWrite(HEAT_PIN, LOW);
        digitalWrite(COOL_PIN, LOW);
        break;
    };
}

/*
 * Returns the current peak
 */
byte get_peak()
{
    // Return NORTC of the rtc is not working
    if (!rtc.isrunning())
    {
        return NO_RTC;
    }
    // Get time from RTC
    DateTime now = rtc.now();
    int tod = now.hour() * 60 + now.minute() * 60 + now.second();
    // Check time for most recent passed peak
    peak_list *pl = peaks;
    while (pl && pl->p.tod < tod)
    {
        pl = pl->next;
    }
    pl = pl->prev;
    return pl->p.peak;
}

/*
 * Returns the control mode from a temp setting
 */
byte temp_setting_ctrl(temp_setting &ts)
{
    if (ts.time < 0)
    {
        return CONTROL_PEAK;
    }
    else
    {
        return CONTROL_TIME;
    }
}

/*
 * Expands temperature from its single-byte compressed form into a float
 */
float expand_temp(byte ctemp)
{
    float temp = 0;
    // Extract .5 from unused MSB (atmega328p is LE)
    if (ctemp & 0b00000001)
    {
        temp += .5;
    }
    // Extract the rest of the temperature data
    ctemp &= 0b11111110;
    temp += ctemp;

    return temp;
}

/*
 * Compresses temperature from a float into a single-byte compressed form
 */
byte compress_temp(float temp)
{
    byte ctemp = floor(temp);
    // Encode .5 into unused MSB
    if (temp - floor(temp) == .5)
    {
        ctemp |= 0b00000001;
    }
    return ctemp;
}

/*
 * Converts from celsius to farenheit
 */
float convert_farenheit(float celsius)
{
    return celsius * (9. / 5.) + 32.;
}

/*
 * Rounds n to nearest .5
 */
float round_to_half(float n)
{
    if (n - floor(n) < .5)
    {
        return floor(n);
    }
    else
    {
        return ceil(n);
    }
}
