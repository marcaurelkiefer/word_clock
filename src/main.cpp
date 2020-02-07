#include <ESP8266WiFi.h> 
#include <DNSServer.h>        
#include <ESP8266WebServer.h> 
#include <Adafruit_NeoPixel.h>
#include <WifiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <time.h>

//GPIO pins
#define D0 16
#define D1 5 // I2C Bus SCL (clock)
#define D2 4 // I2C Bus SDA (data)
#define D3 0
#define D4 2  // Same as "LED_BUILTIN", but inverted logic
#define D5 14 // SPI Bus SCK (clock)
#define D6 12 // SPI Bus MISO
#define D7 13 // SPI Bus MOSI
#define D8 15 // SPI Bus SS (CS)
#define D9 3  // RX0 (Serial console)
#define D10 1 // TX0 (Serial console)

#define NUM_PIXELS 112 //number of LEDs

//NTP constants
#define ONE_HOUR 3600
#define UNIX_OFFSET   946684800
#define NTP_OFFSET   3155673600

WiFiUDP ntpUDP;
WiFiManager wifiManager;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 0, 60000);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_PIXELS, D5, NEO_GRB + NEO_KHZ800);

uint16_t HUE = 65535; //Color of the LEDs, initialize to red
uint8_t SATURATION = 0; //Saturation of LEDs
uint8_t BRIGHTNESS = 5; //Brightness of LEDs

struct tm * time_struct; //time structure for DST correction

enum _MONTHS_
{
  JANUARY,
  FEBRUARY,
  MARCH,
  APRIL,
  MAY,
  JUNE,
  JULY,
  AUGUST,
  SEPTEMBER,
  OCTOBER,
  NOVEMBER,
  DECEMBER
};


unsigned long correct_dst(const unsigned long epoch_time)
{
  struct tm tmptr;
  uint8_t month, mday, hour, day_of_week, d;
  int n;
  time_t timer = epoch_time;
  /* obtain the variables */
  gmtime_r(&timer, &tmptr);
  month = tmptr.tm_mon;
  day_of_week = tmptr.tm_wday;
  mday = tmptr.tm_mday - 1;
  hour = tmptr.tm_hour;

  if ((month > MARCH) && (month < OCTOBER))
    return epoch_time + 7200;

  if (month < MARCH)
    return epoch_time + 3600;
  if (month > OCTOBER)
    return epoch_time + 3600;

  /* determine mday of last Sunday */
  n = tmptr.tm_mday - 1;
  n -= day_of_week;
  n += 7;
  d = n % 7; /* date of first Sunday */

  n = 31 - d;
  n /= 7; /* number of Sundays left in the month */

  d = d + 7 * n; /* mday of final Sunday */

  if (month == MARCH)
  {
    if (d < mday)
      return epoch_time + 3600;
    if (d > mday)
      return epoch_time + 7200;
    if (hour < 1)
      return epoch_time + 3600;
    return epoch_time + 7200;
  }
  if (d < mday)
    return epoch_time + 7200;
  if (d > mday)
    return epoch_time + 3600;
  if (hour < 1)
    return epoch_time + 7200;
  return epoch_time + 3600;
}
struct tm *getTm()
{
  time_t rawtime = correct_dst(timeClient.getEpochTime());
  return localtime (&rawtime);
}
unsigned int getYear()
{
  return getTm()->tm_year + 1900;
}
unsigned int getMonth()
{
  return getTm()->tm_mon + 1;
}
unsigned int getDay()
{
  return getTm()->tm_mday;
}
unsigned int getHour()
{
  return getTm()->tm_hour;
}
unsigned int getMinute()
{
  return getTm()->tm_min;
}
unsigned int getSecond()
{
  return getTm()->tm_sec;
}
String getFullFormattedTime() {
   time_t rawtime = correct_dst(timeClient.getEpochTime());
   struct tm * ti;
   ti = localtime (&rawtime);

   uint16_t year = ti->tm_year + 1900;
   String yearStr = String(year);

   uint8_t month = ti->tm_mon + 1;
   String monthStr = month < 10 ? "0" + String(month) : String(month);

   uint8_t day = ti->tm_mday;
   String dayStr = day < 10 ? "0" + String(day) : String(day);

   uint8_t hours = ti->tm_hour;
   String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

   uint8_t minutes = ti->tm_min;
   String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

   uint8_t seconds = ti->tm_sec;
   String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

   return yearStr + "-" + monthStr + "-" + dayStr + " " +
          hoursStr + ":" + minuteStr + ":" + secondStr;
}

//turn on pixel
void on(int pixel)
{
  pixels.setPixelColor(pixel,Adafruit_NeoPixel::ColorHSV(HUE, SATURATION, BRIGHTNESS));
}

#define _WIFI {on(0);on(1);on(6);on(7);};
#define _NTP {on(20);on(21);on(22);};

//MINUTEN
#define _M_FUENF {on(5);on(4);on(3);on(2);};
#define _M_ZEHN {on(8);on(9);on(10);on(11);};
#define _M_VIERTEL {on(23);on(24);on(25);on(26);on(27);on(28);on(29);};
#define _M_ZWANZIG {on(19);on(18);on(17);on(16);on(15);on(14);on(13);};

//ZWISCHENWORTE
#define _VOR {on(41);on(40);on(39);};
#define _NACH {on(38);on(37);on(36);on(35);};
#define _HALB {on(33);on(32);on(31);on(30);};
#define _PUNKT {on(43);on(44);on(45);on(46);on(47);};

//STUNDEN
#define _H_EINS {on(68);on(67);on(66);on(65);};
#define _H_ZWEI {on(96);on(97);on(98);on(99);};
#define _H_DREI {on(90);on(89);on(88);on(87);};
#define _H_VIER {on(70);on(71);on(72);on(73);};
#define _H_FUENF {on(103);on(102);on(101);on(100);};
#define _H_SECHS {on(74);on(75);on(76);on(77);on(78);};
#define _H_SIEBEN {on(50);on(51);on(52);on(53);on(54);on(55);};
#define _H_ACHT {on(92);on(93);on(94);on(95);};
#define _H_NEUN {on(85);on(84);on(83);on(82);};
#define _H_ZEHN {on(59);on(58);on(57);on(56);};
#define _H_ELF {on(105);on(104);on(103);};
#define _H_ZWOELF {on(64);on(63);on(62);on(61);on(60);};

#define _UHR {on(106);on(107);on(108);};
#define _ERROR_M {on(9);on(26);on(39);on(62);on(73);on(49);};
#define _ERROR_H {on(9);on(26);on(39);on(62);on(73);on(107);};

void showTime()
{
  unsigned int min = getMinute();
  unsigned int hour = getHour();

  if (min < 5)
  {
    _PUNKT
  }
  else if (min < 10)
  {
    _M_FUENF
    _NACH
  }
  else if (min < 15)
  {
    _M_ZEHN
    _NACH
  }
  else if (min < 20)
  {
    _M_VIERTEL
    _NACH
  }
  else if (min < 25)
  {
    _M_ZWANZIG
    _NACH
  }
  else if (min < 30)
  {
    _M_FUENF
    _VOR
    _HALB
  }
  else if (min < 35)
  {
    _HALB
  }
  else if (min < 40)
  {
    _M_FUENF
    _NACH
    _HALB
  }
  else if (min < 45)
  {
    _M_ZWANZIG
    _VOR
  }
  else if (min < 50)
  {
    _M_VIERTEL
    _VOR
  }
  else if (min < 55)
  {
    _M_ZEHN
    _VOR
  }
  else if (min < 60)
  {
    _M_FUENF
    _VOR
  }
  else
  {
    _ERROR_M
  }
  
  
  if ((hour % 12) == 0)
  {
    if (min < 25) {_H_ZWOELF}
    else {_H_EINS}
  }
  else if ((hour % 12) == 1)
  {
    if (min < 25) {_H_EINS}
    else {_H_ZWEI}
  }
  else if ((hour % 12) == 2)
  {
    if (min < 25) {_H_ZWEI}
    else {_H_DREI}
  }
  else if ((hour % 12) == 1)
  {
    if (min < 25) {_H_EINS}
    else {_H_ZWEI}
  }
  else if ((hour % 12) == 2)
  {
    if (min < 25) {_H_ZWEI}
    else {_H_DREI}
  }
  else if ((hour % 12) == 3)
  {
    if (min < 25) {_H_DREI}
    else {_H_VIER}
  }
  else if ((hour % 12) == 4)
  {
    if (min < 25) {_H_VIER}
    else {_H_FUENF}
  }
  else if ((hour % 12) == 5)
  {
    if (min < 25) {_H_FUENF}
    else {_H_SECHS}
  }
  else if ((hour % 12) == 6)
  {
    if (min < 25) {_H_SECHS}
    else {_H_SIEBEN}
  }
  else if ((hour % 12) == 7)
  {
    if (min < 25) {_H_SIEBEN}
    else {_H_ACHT}
  }
  else if ((hour % 12) == 8)
  {
    if (min < 25) {_H_ACHT}
    else {_H_NEUN}
  }
  else if ((hour % 12) == 9)
  {
    if (min < 25) {_H_NEUN}
    else {_H_ZEHN}
  }
  else if ((hour % 12) == 10)
  {
    if (min < 25) {_H_ZEHN}
    else {_H_ELF}
  }
  else if ((hour % 12) == 11)
  {
    if (min < 25) {_H_ELF}
    else {_H_ZWOELF}
  }
  else
  {
    _ERROR_H
  }
  

}

void setup()
{
  Serial.begin(9600);
  wifiManager.autoConnect("WORD CLOCK");
  ArduinoOTA.setHostname("Word_Clock_OTA");
  ArduinoOTA.begin();
  timeClient.begin();

  //clear pixels
  pixels.begin();
  pixels.show();

  Serial.println("Hallo OTA");
  
  

  pixels.show();
}

// the loop function runs over and over again forever
void loop()
{
  ArduinoOTA.handle();
  timeClient.update();
  pixels.clear();
  showTime();
  pixels.show();
}