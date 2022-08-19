// версия с переписанным кодом, оптимизированы некоторые функции
// уменьшено количество переменных, добавлен правильный вывод
// убраны функции работы с внешней EEPROM на часах реального времени
// скетч полностью рабочий с правильным выводом в порт, только парсить
// Watchdog данная версия не поддерживает


#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <RTClib.h>
#include <math.h>
#include <MD_MAX72xx.h>
#include <SoftwareSerial.h>


LiquidCrystal_I2C lcd(0x27, 20, 4);
Adafruit_ADS1X15 ads;
RTC_DS3231 rtc;
SoftwareSerial OpenLogSerial(8, 9);

#define LCD_Period 1000
#define Serial_Period 2000
#define LED_Matrix_Period 1000
#define Serial_Speed 9600
#define RegulatorPeriod 10000
#define AirConst 1.0
#define H2Const 0.47
#define HeConst 0.18
#define NeConst 0.25
#define ArConst 1.31
#define KrConst 1.98
#define XeConst 2.71
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW //type of scheme on LED panel
#define MAX_DEVICES 8
#define CLK_PIN 12 //SCK
#define DATA_PIN 11 //MOSI
#define CS_PIN 10 //SS
#define CHAR_SPACING 1 //pixels between characters on LED panel
#define BUF_SIZE 75

volatile boolean btnStartFlag = false;
volatile boolean btnChangeFlag = false;
volatile uint32_t debounce;
uint32_t tmr = 0; //timer
uint32_t tmr1 = 0;
uint32_t tmrRegulator = 0;
float k = 0.1; //filter running avarage couficient 0.0-1.0
int8_t RegulatorState = 0;
float Voltage = 0.0;
int16_t adc0 = 0;
boolean MStartMeasurment = false;
int8_t GasNum = 0;
float Gas = AirConst;
struct GasConstant {
  String GConstText;
  float GConstValue;
};
GasConstant GConst[7];
char message[BUF_SIZE] = "Hello!"; //test message for LED Panel
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

void setup() {
  GConst[0] = (GasConstant) {
    F("N2"), AirConst
  };
  GConst[1] = (GasConstant) {
    F("H2"), H2Const
  };
  GConst[2] = (GasConstant) {
    F("He"), HeConst
  };
  GConst[3] = (GasConstant) {
    F("Ne"), NeConst
  };
  GConst[4] = (GasConstant) {
    F("Ar"), ArConst
  };
  GConst[5] = (GasConstant) {
    F("Kr"), KrConst
  };
  GConst[6] = (GasConstant) {
    F("Xe"), XeConst
  };
  ads.setGain(GAIN_SIXTEEN);
  ads.begin();
  int16_t adc0;
  lcd.init();
  lcd.backlight();
  //rtc.begin();
  Serial.begin(Serial_Speed);
  OpenLogSerial.begin(Serial_Speed);
  pinMode(3, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(0, btnStart, CHANGE);
  attachInterrupt(1, btnChange, CHANGE);
  if (! rtc.begin()) {
    Serial.println("DS3231 not found!!!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Real time clock");
    lcd.setCursor(0, 1);
    lcd.print("DS3231");
    lcd.setCursor(0, 2);
    lcd.print("NOT FOUND!!!");
    for (;;);
  }
  if (rtc.lostPower()) {
    Serial.println("DS3231 lost power! ");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DS3231 Lost power");
    // rtc.setTime(COMPILE_TIME);
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    delay(5000);
  }
  for (byte i = 4; i <= 7; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, HIGH);
  }
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  delay(1000);
  lcd.clear();
  MStartMeasurment = false;
  btnStartFlag = false;
  btnChangeFlag = false;
  mx.begin();
  delay(1000);
  mx.clear();
}

void loop() {
  if (btnChangeFlag == true) {
    btnChangeFlag = false;
    if (GasNum >= 6) {
      GasNum = 0;
    } else {
      GasNum++;
    }
  }
  if (btnStartFlag == true) {
    btnStartFlag = false;
    MStartMeasurment = not MStartMeasurment;
    RegulatorState = 0;
    lcd.clear();
  }
  if (MStartMeasurment) {
    adc0 = ads.readADC_Differential_0_1(); //меряем напряжение
    Voltage = (adc0 * 0.078125);
    Regulation();
  }
  if (millis() - tmr >= Serial_Period) {
    tmr = millis();
    OutSerial();
    OutOpenLog();
  }
  if (millis() - tmr1 >= LCD_Period) {
    tmr1 = millis();
    OutLCD();
    OutLEDPanel();
  }
}

void btnStart() {
  if (millis() - debounce >= 200 && digitalRead(2)) {
    debounce = millis();
    digitalWrite(4, HIGH);
    digitalWrite(5, HIGH);
    digitalWrite(6, HIGH);
    digitalWrite(7, HIGH);
    RegulatorState = 0;
    btnStartFlag = true;
  }
}

void btnChange() {
  if (millis() - debounce >= 200 && digitalRead(3)) {
    debounce = millis();
    btnChangeFlag = true;
  }
}

float expRunningAverage(float newVal) {
  static float filVal = 0;
  filVal += (newVal - filVal) * k;
  return filVal;
}

void OutSerial() {

  DateTime now = rtc.now();


  Serial.print(now.day(), DEC);
  Serial.print("/");
  Serial.print(now.month(), DEC);
  Serial.print("/");
  Serial.print(now.year(), DEC);
  Serial.print(",");
  Serial.print(now.hour(), DEC);
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.print(now.second());
  Serial.print(",TempRTC=,");
  Serial.print(rtc.getTemperature());
  if (MStartMeasurment) {
    Serial.print(",ADC=,");
    Serial.print(adc0, 5);
    Serial.print(",I=,");
    Serial.print(Voltage, 7);
    Serial.print(",");
    Serial.print("Mode:,");
    if (RegulatorState > 1) Serial.print("-");
    Serial.print(RegulatorState - 1);
    Serial.print(",GasConstant:,");
    Serial.print(GConst[GasNum].GConstText);
    Serial.print(",VacuumValue=,");
    Serial.print(VacuumValue(Voltage, RegulatorState, GConst[GasNum].GConstValue), 10);
    Serial.print(",VacuumValueExp=,");
    Serial.print(VacuumValueToExp(VacuumValue(Voltage, RegulatorState, GConst[GasNum].GConstValue)));
    Serial.println();
  } else {
    Serial.println(" --Press START button for measurment");
  }
}

void OutOpenLog() {
  if (MStartMeasurment){
    DateTime now = rtc.now();
    OpenLogSerial.print("Date:");
    OpenLogSerial.print("\t");
    OpenLogSerial.print(now.day(), DEC);
    OpenLogSerial.print(".");
    OpenLogSerial.print(now.month(), DEC);
    OpenLogSerial.print(".");
    OpenLogSerial.print(now.year(), DEC);
    OpenLogSerial.print("\t");
    OpenLogSerial.print("Time:");
    OpenLogSerial.print("\t");
    OpenLogSerial.print(now.hour(), DEC);
    OpenLogSerial.print(":");
    OpenLogSerial.print(now.minute(), DEC);
    OpenLogSerial.print(":");
    OpenLogSerial.print(now.second(), DEC);
    OpenLogSerial.print("\t");
    OpenLogSerial.print("Gas coeficient:");
    OpenLogSerial.print("\t");
    OpenLogSerial.print(GConst[GasNum].GConstText);
    OpenLogSerial.print("\t");
    OpenLogSerial.print("Vacuum value:");
    OpenLogSerial.print("\t");
    OpenLogSerial.print(VacuumValue(Voltage, RegulatorState, GConst[GasNum].GConstValue), 10);
    OpenLogSerial.println();
  }
}

void OutLCD() {

  DateTime now = rtc.now();

  lcd.setCursor(0, 0);
  if (now.day() < 10) lcd.print("0");
  lcd.print(now.day());
  lcd.print("/");
  if (now.month() < 10) lcd.print("0");
  lcd.print(now.month());
  lcd.print("/");
  lcd.print(now.year());
  lcd.setCursor(11, 0);
  if (now.hour() < 10) lcd.print("0");
  lcd.print(now.hour());
  lcd.print(":");
  if (now.minute() < 10) lcd.print("0");
  lcd.print(now.minute());
  lcd.print(":");
  if (now.second() < 10) lcd.print("0");
  lcd.print(now.second());
  lcd.setCursor(0, 1);
  lcd.print("set gas: ");
  lcd.print(GConst[GasNum].GConstText);
  if (MStartMeasurment) {
    lcd.setCursor(0, 2);
    if (RegulatorState < 1) {
      lcd.print(" ");
    } else {
      lcd.print("Iu=");
      lcd.print(" ");
      if (Voltage < 100) lcd.print(" ");
      if (Voltage < 10) lcd.print(" ");
      lcd.print(Voltage, 5);
      lcd.print("x10^");
      if (RegulatorState > 1) lcd.print("-");
      lcd.print(RegulatorState - 1);
      if (Voltage < 10 && Voltage > 0) lcd.print(" ");
      if (Voltage > 10 && Voltage < 100) lcd.print(" ");
    }
    lcd.setCursor(0, 3);
    lcd.print("P=");
    lcd.print(VacuumValueToExp(VacuumValue(Voltage, RegulatorState, GConst[GasNum].GConstValue)));
    lcd.print(" Top ");
  } else {
    lcd.setCursor(0, 2);
    lcd.print("Press START button");
    lcd.setCursor(0, 3);
    lcd.print("for start measurment");
  }
}

void OutLEDPanel() {
  char expToCharArray[11];
  if (MStartMeasurment) {
    //expToCharArray[4] = "e";
    expToCharArray[8] = 'T';
    expToCharArray[9] = 'o';
    expToCharArray[10] = 'p';
    String expVacuum = VacuumValueToExp(VacuumValue(Voltage, RegulatorState, GConst[GasNum].GConstValue));
    for (byte i = 0; i <= 7; i++) {
      expToCharArray[i] = expVacuum[i];
      //expToCharArray[7 - i] = expVacuum[expVacuum.length() - i - 3];
    }
  }
  else {
    DateTime now = rtc.now();
    //String showClock = String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
    String showClock = ((now.hour() < 10) ? ("0" + String(now.hour())) : (String(now.hour()))) + ":" +
                       ((now.minute() < 10) ? ("0" + String(now.minute())) : (String(now.minute()))) + ":" +
                       ((now.second() < 10) ? ("0" + String(now.second())) : (String(now.second())));    
    for (byte i = 0; i < 11; i++) expToCharArray[i] = ' ';
    for (byte i = 0; i < showClock.length(); i++) expToCharArray[i] = showClock[i];
  }

  printText(0, MAX_DEVICES - 1, expToCharArray);
}

void Regulation() {
  if (((Voltage < 8) or (Voltage > 95)) and (millis() - tmrRegulator >= RegulatorPeriod)) {
    tmrRegulator = millis();
    switch (RegulatorState) {
      case 0: {
          //0000
          if (Voltage < 8) {
            RegulatorState = 1;
            digitalWrite(4, LOW);
            digitalWrite(5, HIGH);
            digitalWrite(6, HIGH);
            digitalWrite(7, LOW);
          }
          break;
        }
      case 1: {
          //1001
          if (Voltage < 8) {
            RegulatorState = 2;
            digitalWrite(4, LOW);
            digitalWrite(5, LOW);
            digitalWrite(6, LOW);
            digitalWrite(7, HIGH);
          }
          break;
        }
      case 2: {
          //1110
          if (Voltage > 95) {
            RegulatorState = 1;
            digitalWrite(4, LOW);
            digitalWrite(5, HIGH);
            digitalWrite(6, HIGH);
            digitalWrite(7, LOW);
          }
          if (Voltage < 8) {
            RegulatorState = 3;
            digitalWrite(4, LOW);
            digitalWrite(5, HIGH);
            digitalWrite(6, LOW);
            digitalWrite(7, HIGH);
          }
          break;
        }
      case 3: {
          //1010
          if (Voltage > 95) {
            RegulatorState = 2;
            digitalWrite(4, LOW);
            digitalWrite(5, LOW);
            digitalWrite(6, LOW);
            digitalWrite(7, HIGH);
          }
          if (Voltage < 8) {
            RegulatorState = 4;
            digitalWrite(4, LOW);
            digitalWrite(5, LOW);
            digitalWrite(6, HIGH);
            digitalWrite(7, HIGH);
          }
          break;
        }
      case 4: {
          //1100
          if (Voltage > 95) {
            RegulatorState = 3;
            digitalWrite(4, LOW);
            digitalWrite(5, HIGH);
            digitalWrite(6, LOW);
            digitalWrite(7, HIGH);
          }
          if (Voltage < 8) {
            RegulatorState = 5;
            digitalWrite(4, LOW);
            digitalWrite(5, HIGH);
            digitalWrite(6, HIGH);
            digitalWrite(7, HIGH);
          }
          break;
        }
      case 5: {
          //1000
          if (Voltage > 95) {
            RegulatorState = 4;
            digitalWrite(4, LOW);
            digitalWrite(5, LOW);
            digitalWrite(6, HIGH);
            digitalWrite(7, HIGH);
          }
          break;
        }
    }
  }
}

float VacuumValue(float AnodCurrent, int8_t RegState, float constGas) {
  return ((87 / constGas) * ((AnodCurrent / 100000) * pow(10, -RegState)));
}

String VacuumValueToExp(float VacVal) {
  static char outstr[15];
  return (dtostre(VacVal, outstr, 2, 0));
}

void printText(uint8_t modStart, uint8_t modEnd, char *pMsg) {
  // Print the text string to the LED matrix modules specified.
  // Message area is padded with blank columns after printing.
  uint8_t state = 0;
  uint8_t curLen;
  uint16_t showLen;
  uint8_t cBuf[8];
  int16_t col = ((modEnd + 1) * COL_SIZE) - 1;

  mx.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  do // finite state machine to print the characters in the space available
  {
    switch (state)
    {
      case 0: // Load the next character from the font table
        // if we reached end of message, reset the message pointer
        if (*pMsg == '\0')
        {
          showLen = col - (modEnd * COL_SIZE); // padding characters
          state = 2;
          break;
        }

        // retrieve the next character form the font file
        showLen = mx.getChar(*pMsg++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);
        curLen = 0;
        state++;
      // !! deliberately fall through to next state to start displaying

      case 1: // display the next part of the character
        mx.setColumn(col--, cBuf[curLen++]);

        // done with font character, now display the space between chars
        if (curLen == showLen)
        {
          showLen = CHAR_SPACING;
          state = 2;
        }
        break;

      case 2: // initialize state for displaying empty columns
        curLen = 0;
        state++;
      // fall through

      case 3: // display inter-character spacing or end of message padding (blank columns)
        mx.setColumn(col--, 0);
        curLen++;
        if (curLen == showLen)
          state = 0;
        break;

      default:
        col = -1; // this definitely ends the do loop
    }
  } while (col >= (modStart * COL_SIZE));

  mx.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}
