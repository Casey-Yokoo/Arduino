/*
  A temperature control program to improve the quality of natto. 
  20230806 Kiyoshi Yokoo

  Inspired by, 「糸引納豆製造法の改良」
  "Improvement of Itohiki-Natto-Manufacturing Process" 
    Kanako MURAMATSU*,**, Rie KATSUMATA*, Sugio WATANABE***, Tadayoshi TANAKA****, Kan KIUCHI*
    Nippon Shokuhin Kagaku Kogaku Kaishi Vol. 48, No. 4, 277～286 (2001)
  "to set the temperature initially at 42℃ , to rise it up to 47℃ for 3 or 4 hours on the way 
  and to decrease it to 5℃ at the 20th hour ......."
  「納豆を作る時は24時間かかりますが、8時間目まで42℃に。8時間目から温度を上げて10時間目から47℃に4時間保ち、
  その後再び温度を下げて42℃に保ち、さらに20時間目から温度 を下げて4時間目に4℃にすることによって品質がよくなる」
  https://www.jstage.jst.go.jp/article/nskkk1995/48/4/48_4_277/_pdf
*/
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>

// Natto terms and temperature. 
#define TERM_01 0 // Natto term initially
#define TERM_02 10 // Natto term 02
#define TERM_03 14 // Natto term 03
#define TERM_04 24 // Natto term INFINITY
#define TERM_TEMP_01 44.5 // Temperature initially
#define TERM_TEMP_02 46.5 // Temperature term 02
#define TERM_TEMP_03 41.5 // Temperature term 03
float tempT1 = TERM_TEMP_01;
float tempC1;
unsigned long time_lastChecked;
unsigned long time;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
// #define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
//#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
//OLED Display SSD1306用軽いキャラクタライブラリー
#include "SSD1306Ascii.h"
#include "SSD1306AsciiAvrI2c.h"
#define I2C_ADDRESS 0x3C
// #define RST_PIN -1
SSD1306AsciiAvrI2c oled;
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Pass our oneWire reference to Dallas Temperature. 
#define ONE_WIRE_BUS 5 // Temperature Sensor
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
// DeviceAddress temp1 = {0x28,0xFF,0x70,0x3E,0xA8,0x15,0x3,0x5A};
DeviceAddress temp1 = {0x28,0x10,0x1F,0x49,0xF6,0xD4,0x3C,0x7};
#define TEMP_INTERVAL 3000

// Auto / Manual
#define AUTO_BUS 4
#define SIGN_AUTO "AUTO"
#define SIGN_MANUAL "MAN "
boolean auto_man = true;
unsigned long lastButtonPress = 0;

// Encoder
#define ENC_BUS_CLK 2
#define ENC_BUS_DT 3
#define ENC_TEMP_HIGH 50
#define ENC_TEMP_LOW 20
int encoder_clk;
int encoder_clk_last;

// For relay. 
#define RELAY_BUS 6
boolean relay_state = true;
boolean relay_state_old = true;

void setup() {
  // For relay. 
  pinMode(RELAY_BUS, OUTPUT);

  // Auto / Manual 
  pinMode(AUTO_BUS, INPUT_PULLUP);

  // Encoder
  pinMode (ENC_BUS_CLK, INPUT);
  pinMode (ENC_BUS_DT, INPUT);
  encoder_clk_last = digitalRead(ENC_BUS_CLK);

  oled.begin(&Adafruit128x32, I2C_ADDRESS);  //こっちにすると文字高さ２倍
  //  oled.begin(&Adafruit128x64, I2C_ADDRESS);  //これは小さくぎっちりサイズ
  oled.setFont(System5x7);      //文字フォント調べるか
  // oled.setFont(TimesNewRoman16);
  oled.clear();

  // Start up the DallasTemperature library
  sensors.begin();

  // Open serial communications and wait for port to open:
  // Serial.begin(9600);
  // while (!Serial) {
  //   ; // wait for serial port to connect. Needed for native USB port only
  // }
}

void loop() {
  // Current time
  unsigned short int time_min;
  unsigned short int time_hour;
  time = millis();//Running time(ms)
  time_min = time/60000;
  time_hour = time_min/60;
  time_min = time_min-time_hour*60;

  // Auto / Manual
  if(digitalRead(AUTO_BUS) == LOW ){
    if (millis() - lastButtonPress > 500) {
      auto_man = !auto_man;
      tempT1 = floor(tempT1);
      encoder_clk_last = encoder_clk;
      oled.clear();
    }
    lastButtonPress = millis();
  }
  String auto_man_sign = SIGN_MANUAL;
  if (auto_man == 1) {
    auto_man_sign = SIGN_AUTO;
    tempT1 = TERM_TEMP_01;
    if(time_hour >= TERM_02)
    {
      tempT1 = TERM_TEMP_02;
    }
    if(time_hour >= TERM_03)
    {
      tempT1 = TERM_TEMP_03;
    }
    getTemp();
  }
  else
  { 
    // Get encoder val
    float delta_target_temp = 0;
    encoder_clk = digitalRead(ENC_BUS_CLK);
    if (encoder_clk != encoder_clk_last) {
      if (encoder_clk != digitalRead(ENC_BUS_DT)) {
        delta_target_temp = 1;
      } else {
        delta_target_temp = -1;
      }
      encoder_clk_last = encoder_clk;
    }
      else
    {
      getTemp();
    }
    if (delta_target_temp != 0) {
      tempT1 = tempT1 + delta_target_temp;
      if(tempT1 < ENC_TEMP_LOW){
        tempT1 = ENC_TEMP_LOW;
      }
      if(tempT1 > ENC_TEMP_HIGH){
        tempT1 = ENC_TEMP_HIGH;
      }       
      time_lastChecked = time;
    }
  }

  // Relay operation
  relay_state = true;
  if(tempC1 < tempT1)
  {
    relay_state = false;
  } 
  if (relay_state != relay_state_old) {
    digitalWrite(RELAY_BUS, relay_state);
    relay_state_old = relay_state;
  }
  
  // print to the OLED:
  String oledString1;
  String oledString2;

  oledString1 = strPad2(time_hour,2);
  oledString1 += ":";
  oledString1 += strPad2(time_min,2);
  oledString1 += " "; 
  oledString1 += String(tempC1,1);

  oledString2 = auto_man_sign;
  oledString2 += "  "; 
  oledString2 += String(tempT1,1); 

  oled.set2X();
  oled.setCursor(1,0);
  oled.println(oledString1);
  oled.setCursor(1,2);
  oled.println(oledString2);

  // For debug
  // String debugString;
  // debugString += String(time_min);
  // debugString += ",";
  // debugString += tempC1;
  // debugString += ",";
  // debugString += auto_man;
  // Serial.println(debugString);

  //delay(60000);//60000msec待機(60秒待機)
  //delay(3000);//3000msec待機(3秒待機)
}

// numに数値を、zeroCountに桁数を指定する。
String strPad2(unsigned int num,unsigned int zeroCount){
  char tmp[256];
  char param[5] = {'%','0',(char)(zeroCount+48),'d','\0'};
  sprintf(tmp,param,num);
  return tmp;
}

void getTemp(){
  if (time - time_lastChecked > TEMP_INTERVAL){
    // call Temperaturesensors
    sensors.requestTemperatures(); // Send the command to get temperatures
    tempC1 = sensors.getTempC(temp1);
    time_lastChecked = time;
  }
}
