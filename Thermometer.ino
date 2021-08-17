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

#include <EEPROMWearLevel.h>

#define SERIAL_RATE 9600

//Display config
#define LCD_COLS 16
#define LCD_ROWS 2
#define FLASH_DURATION 4000

//Sensor config
#define DHT_TYPE DHT22
#define DHT_PIN 2

//Relay config
#define HEAT_PIN 3
#define COOL_PIN 4
#define FAN_PIN 5

//Peak constants
#define OFF_PEAK 0
#define MID_PEAK 1
#define ON_PEAK 2
#define UNKNOWN_PEAK 3
#define NO_RTC 4

//Mode constants
#define MODE_OFF 0
#define MODE_HEAT 1
#define MODE_COOL 2
#define MODE_FAN 3
#define MODE_AUTO 4

//Control constants
#define CONTROL_SIMPLE 0
#define CONTROL_TIME 1
#define CONTROL_PEAK 2

typedef struct {
  long tod;
  byte peak;
} peak;

struct peak_list {
  peak_list* prev;
  peak p;
  peak_list* next;
};

typedef struct {
  //In peak mode, time is the peak value. In time mode, time is the time of day (in seconds.)
  //The first bit in time specifies whether the setting is for peak mode or time mode.
  long time;
  byte temp;
} temp_setting;

struct temp_list {
  temp_list* prev;
  temp_setting ts;
  temp_list* next;
};

peak_list* peaks;
temp_list* temps;

//lcd global vars
bool refresh_lcd = true;
bool farenheit = false;
unsigned long flash_time = 0;
byte old_minute = 0;

//temperature control vars
float temp = 0;
float tgt_temp = 0;
byte mode = MODE_OFF;
bool running = false;

//peak vars
byte old_peak = UNKNOWN_PEAK;

LiquidCrystal_I2C lcd(0x27, LCD_COLS, LCD_ROWS);
DHT dht(DHT_PIN, DHT_TYPE);
RTC_DS1307 rtc;

void update_target_temp();

void update_disp_standby();
void update_disb_menu();

byte get_peak();

byte temp_setting_ctrl(temp_setting&);

float expand_temp(byte);
byte compress_temp(float);

float convert_farenheit(float);
float round_to_half(float);

void setup() {
  pinMode(HEAT_PIN, OUTPUT);
  pinMode(COOL_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Starting...");
  
  dht.begin();

  EEPROMwl.begin(0, 64);
  //Read settings from EEPROM
  byte ctrl_type = EEPROMwl.read(0);
  mode = EEPROMwl.read(1);
  
  lcd.clear();
  lcd.noBacklight();
}

void loop() {
  unsigned long stime = millis();

  temp = dht.readTemperature();

  //Set relays
  switch (mode) {
    case MODE_HEAT:
      if (temp < tgt_temp - .5) {
        if (!running) {
          flash_time = millis();
          running = true;
        }
        digitalWrite(HEAT_PIN, HIGH);
        digitalWrite(FAN_PIN, HIGH);
      }
      if (temp > tgt_temp + .5) {
        if (running) {
          flash_time = millis();
          running = false;
        }
        digitalWrite(HEAT_PIN, LOW);
        digitalWrite(FAN_PIN, LOW);
      }
      break;
    case MODE_COOL:
      if (temp > tgt_temp + .5) {
        if (!running) {
          flash_time = millis();
          running = true;
        }
        digitalWrite(COOL_PIN, HIGH);
        digitalWrite(FAN_PIN, HIGH);
      }
      if (temp < tgt_temp - .5) {
        if (running) {
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
      //Heat on
      if (temp < tgt_temp - .5) {
        if (!running) {
          flash_time = millis();
          running = true;
          refresh_lcd = true;
        }
        digitalWrite(HEAT_PIN, HIGH);
        digitalWrite(FAN_PIN, HIGH);
      }
      //Heat off
      if (!digitalRead(COOL_PIN) && temp > tgt_temp) {
        if (running) {
          flash_time = millis();
          running = false;
          refresh_lcd = true;
        }
        digitalWrite(HEAT_PIN, LOW);
        digitalWrite(FAN_PIN, LOW);
      }
      //Cool on
      if (temp > tgt_temp + .5) {
        if (!running) {
          flash_time = millis();
          running = true;
          refresh_lcd = true;
        }
        digitalWrite(COOL_PIN, HIGH);
        digitalWrite(FAN_PIN, HIGH);
      }
      if (!digitalRead(HEAT_PIN) && temp < tgt_temp) {
        if (running) {
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

  //
  
  //Update LCD if time changes
  if (rtc.isrunning()) {
    DateTime now = rtc.now();
    if (now.minute() != old_minute) {
      refresh_lcd = true;
      old_minute = now.minute();
    }
  }
  
  //Set display backlight
  if (millis() - flash_time > FLASH_DURATION) {
    lcd.backlight();
  }
  else {
    lcd.noBacklight();
  }
  if (refresh_lcd) {
    update_disp_standby();
    refresh_lcd = false;
  }
  
  delay(2000 - (millis() - stime));
}

/*
 * Function that updates the display in standby mode
 */
void update_disp_standby() {
  lcd.clear();
  //Print peak
  lcd.setCursor(0, 0);
  byte p = get_peak();
  switch (p) {
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

  //Print current temperature on lcd
  float temp = dht.readTemperature();
  if (farenheit) {
    temp = round_to_half(convert_farenheit(temp));
  }
  String tempstr = String(temp);
  lcd.setCursor(LCD_COLS / 2 - tempstr.length() / 2, 0);
  lcd.print(tempstr);
  if (farenheit) {
    lcd.print('F');
  } else {
    lcd.print('C');
  }

  //Print out mode on LCD
  lcd.setCursor(LCD_COLS - 7, 0);
  switch (mode) {
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
  
  //Print out running asterisk
  if (running) {
    lcd.setCursor(LCD_COLS - 1, 0);
    lcd.print('*');
  }

  //Print out target tempreature
  lcd.setCursor(0, 1);
  lcd.print("TGT ");
  lcd.print(tgt_temp);
  if (farenheit) {
    lcd.print('F');
  } else {
    lcd.print('C');
  }

  //Print out time
  lcd.setCursor(LCD_COLS - 7, 1);
  if (rtc.isrunning()) {
    DateTime now = rtc.now();
    lcd.print(String(now.hour()) + ':' + String(now.minute()));
  }
  //Else print NORTC if the rtc is not initialized
  else {
    lcd.print("NORTC");
  }
}

/*
 * Function that updates the display in menu mode
 */
void update_disp_menu() {
  
}

/*
 * Returns the current peak
 */
byte get_peak() {
  //Return NORTC of the rtc is not working
  if (!rtc.isrunning()) {
    return NO_RTC;
  }
  //Get time from RTC
  DateTime now = rtc.now();
  int tod = now.hour() * 60 + now.minute() * 60 + now.second();
  //Check time for most recent passed peak
  peak_list* pl = peaks;
  while (pl && pl->p.tod < tod) {
    pl = pl->next;
  }
  pl = pl->prev;
  return pl->p.peak;
}

/*
 * Returns the control mode from a temp setting
 */
byte temp_setting_ctrl(temp_setting& ts) {
  if (ts.time & (1 >> 31)) {
    return CONTROL_PEAK;
  }
  else {
    return CONTROL_TIME;
  }
}

/*
 * Expands temperature from its single-byte compressed form into a float
 */
float expand_temp(byte ctemp) {
  float temp = 0;
  //Extract .5 from unused MSB (atmega328p is LE)
  if (ctemp & 0b00000001) {
    temp += .5;
  }
  //Extract the rest of the temperature data
  ctemp &= 0b11111110;
  temp += ctemp;
  
  return temp;
}

/*
 * Compresses temperature from a float into a single-byte compressed form
 */
byte compress_temp(float temp) {
  byte ctemp = floor(temp);
  //Encode .5 into unused MSB
  if (temp - floor(temp) == .5) {
    ctemp |= 0b00000001;
  }
  return ctemp;
}

/*
 * Converts from celsius to farenheit
 */
float convert_farenheit(float celsius) {
  return celsius * (9. / 5.) + 32.;
}

/*
 * Rounds n to nearest .5
 */
float round_to_half(float n) {
  if (n - floor(n) < .5) {
    return floor(n);
  } else {
    return ceil(n);
  }
}
