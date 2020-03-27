#include <ESP8266WiFi.h> 
#include <DNSServer.h>        
#include <ESP8266WebServer.h> 
#include <Adafruit_NeoPixel.h>
#include <WifiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <EEPROM.h>
#include <Esp.h>
#include <OneButton.h>

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
#define ANIMATION_MILLIS 2000 //duration of animation in millisecs
#define A_TURN_OFF 1 //Animation turn off
#define A_TURN_ON 2 //Animation turn on
#define A_STAY_ON 3 //Animation stay on

//NTP constants
#define ONE_HOUR 3600
#define UNIX_OFFSET   946684800
#define NTP_OFFSET   3155673600

//moving average samples
#define NUM_SAMPLES 500

#define EEPROM_SIZE 3 //RGB color

WiFiUDP ntpUDP;
WiFiManager wifiManager;
WiFiServer server(80);
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 0, 60000);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_PIXELS, D5, NEO_GRB + NEO_KHZ800);
OneButton button(D7,true);

uint16_t COLOR = 0; //Color of the LEDs, initialize to red
double BRIGHTNESS = 30; //Brightness of LEDs
uint8_t MAX_BRIGHTNESS = 200; //80
uint8_t MIN_BRIGHTNESS = 30; //30
uint8_t pixel_array[NUM_PIXELS] = {0};

struct tm * time_struct; //time structure for DST correction

//webserver
// Variable to store the HTTP request
String header;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;


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
      return epoch_time + 7200;
    if (d > mday)
      return epoch_time + 3600;
    if (hour < 1)
      return epoch_time + 3600;
    return epoch_time + 7200;
  }
  if (d < mday)
    return epoch_time + 3600;
  if (d > mday)
    return epoch_time + 7200;
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



//FARBEN
uint16_t colors[] = {0, 2340, 14040, 21064, 31590, 42120, 47970, 60840, 63765,0};
uint8_t saturations[] = {255,255,255,255,255,255,255,255,255,0};
static_assert(sizeof(colors)/sizeof(uint16_t) == sizeof(saturations)/sizeof(uint8_t),"Size of colors not equal to size of saturations");
int num_colors = sizeof(colors)/sizeof(uint16_t);
#define C_ROT 0
#define C_ORANGE 1
#define C_GELB 2
#define C_GRUEN 3
#define C_CYAN 4
#define C_BLAU 5
#define C_LILA 6
#define C_ROSA 7
#define C_PINK 8

//EEPROM
#define EEPROM_IDX_COLOR 0
#define EEPROM_IDX_SAT 1

//turn on pixel
void on(int pixel)
{

  if (pixel_array[pixel] == 1)
  {
    pixel_array[pixel] = A_STAY_ON; // Stay on
  }
  else
  {
    pixel_array[pixel] = A_TURN_ON; //Turn on
  }
  
  //pixels.setPixelColor(pixel,Adafruit_NeoPixel::ColorHSV(colors[COLOR], saturations[COLOR], 1 + Adafruit_NeoPixel::gamma8(BRIGHTNESS)));
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
#define _H_DREI {Serial.println(getFullFormattedTime());Serial.println(timeClient.getEpochTime());Serial.println(timeClient.getFormattedTime());on(90);on(89);on(88);on(87);};
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
  //sometimes a weird time is returned, workaround!
  if (getYear() < 2020)
  {
    return;
  }
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

void updateDisplay(bool animation = true)
{
  bool update = false;
  //check for display change
  for (int i = 0; i < NUM_PIXELS; i++)
  {
    if (pixel_array[i] == A_TURN_OFF || pixel_array[i] == A_TURN_ON)
    {
      update=true;
      break;
    }
    else
    {
      for (int i = 0; i < NUM_PIXELS; i++)
      {
        if (pixel_array[i] == 1 || pixel_array[i] == 3)
        {
          pixels.setPixelColor(i,Adafruit_NeoPixel::ColorHSV(colors[COLOR], saturations[COLOR], 1 + Adafruit_NeoPixel::gamma8(BRIGHTNESS)));
        }
      }
      pixels.show();
    }
    
  }
  if (update)
  { 
    if (animation)
    {
      Serial.println("--------------------UPDATE-----------------");
      Serial.print("gamma brightness: ");
      Serial.println(1 + Adafruit_NeoPixel::gamma8(BRIGHTNESS));
      for (int bright= 1 + Adafruit_NeoPixel::gamma8(BRIGHTNESS); bright >= 0; bright--)
      {
        for (int i = 0; i < NUM_PIXELS; i++)
        {
          
          if (pixel_array[i] == A_TURN_OFF)
          {
            pixels.setPixelColor(i,Adafruit_NeoPixel::ColorHSV(colors[COLOR], saturations[COLOR], bright));
          }
          if (pixel_array[i] == A_STAY_ON)
          {
            pixels.setPixelColor(i,Adafruit_NeoPixel::ColorHSV(colors[COLOR], saturations[COLOR], 1 + Adafruit_NeoPixel::gamma8(BRIGHTNESS)));
          }
        }
        pixels.show();
        delay(ANIMATION_MILLIS/BRIGHTNESS);
      }
      for (int bright= 0; bright <= 1 + Adafruit_NeoPixel::gamma8(BRIGHTNESS); bright++)
      {
        for (int i = 0; i < NUM_PIXELS; i++)
        {
          
          if (pixel_array[i] == A_TURN_ON)
          {
            pixels.setPixelColor(i,Adafruit_NeoPixel::ColorHSV(colors[COLOR], saturations[COLOR], bright));
          }
          if (pixel_array[i] == A_STAY_ON)
          {
            pixels.setPixelColor(i,Adafruit_NeoPixel::ColorHSV(colors[COLOR], saturations[COLOR], 1 + Adafruit_NeoPixel::gamma8(BRIGHTNESS)));
          }
        }
        pixels.show();
        delay(ANIMATION_MILLIS/BRIGHTNESS);
      }
    }
    else
    {
      for (int i = 0; i < NUM_PIXELS; i++)
      {
        if (pixel_array[i] == A_TURN_ON || pixel_array[i] == A_STAY_ON)
        {
          pixels.setPixelColor(i,Adafruit_NeoPixel::ColorHSV(colors[COLOR], saturations[COLOR], 1 + Adafruit_NeoPixel::gamma8(BRIGHTNESS)));
        }
      }
      pixels.show();
    }
      
    //clean up array
    for (int i = 0; i < NUM_PIXELS; i++)
    {
      //Serial.print(pixel_array[i]);
      
      switch (pixel_array[i])
      {
        case 0:
          //is off, stay off
          break;
        case A_TURN_OFF:
          pixel_array[i] = 0;
          break;
        case A_TURN_ON:
          pixel_array[i] = 1;
          break;
        case A_STAY_ON:
          pixel_array[i] = 1;
          break;
        default:
          Serial.print("pixel_array error value: ");
          Serial.println(pixel_array[i]);
      }
    }
  }
  else
  {
    {
      //clean up array
    for (int i = 0; i < NUM_PIXELS; i++)
    {
      //Serial.print(pixel_array[i]);
      
      if (pixel_array[i] == A_STAY_ON)
      {
        pixel_array[i] = 1;
      } 
    }
    }
  }
  
}
void colorPicker()
{
  for (int i = 0; i < NUM_PIXELS; i++)
  {
    //pixels.setPixelColor(i,Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV((65535/NUM_PIXELS)*i,255,30)));
    pixels.setPixelColor(i,Adafruit_NeoPixel::ColorHSV((65535/NUM_PIXELS)*i,255,30));
  }
  pixels.show();
}

void updateBrightness(bool instant = false) {
    double new_brightness = ((analogRead(A0) * (MAX_BRIGHTNESS - MIN_BRIGHTNESS)) / 1024) + MIN_BRIGHTNESS;
    //Serial.print("new brightness: ");
    //Serial.println(new_brightness);
    if (instant)
    {
      BRIGHTNESS = new_brightness;
    }
    else
    {
      BRIGHTNESS -= BRIGHTNESS / NUM_SAMPLES;
      BRIGHTNESS += new_brightness / NUM_SAMPLES;
    }
}

void setColor(uint8_t color)
{
  if (color < num_colors)
  {
    COLOR = color;
    if (EEPROM.read(EEPROM_IDX_COLOR) != color)
    {
      EEPROM.write(EEPROM_IDX_COLOR, color);
      EEPROM.commit();
    }
  }
}

void initColor()
{
  uint8_t color = EEPROM.read(EEPROM_IDX_COLOR);
  COLOR = color;
}



boolean wifiConnect()
{
  WiFi.mode(WIFI_STA);

  if (wifiManager.connectWifi("", "") == WL_CONNECTED)   {
    //connected
    return true;
  }

  return false;
}

void handleWebserver()
{
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= timeoutTime) { // loop while the client's connected
      yield();
      currentTime = millis();         
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs on and off
            
            if (header.indexOf("GET /color/gruen") >= 0) {
              Serial.println("Setze Farbe Grün");
              setColor(C_GRUEN);
            }
            else if (header.indexOf("GET /color/rot") >= 0) {
              Serial.println("Setze Farbe Rot");
              setColor(C_ROT);
            }
            else if (header.indexOf("GET /color/blau") >= 0) {
              Serial.println("Setze Farbe Blau");
              setColor(C_BLAU);
            }
            else if (header.indexOf("GET /color/cyan") >= 0) {
              Serial.println("Setze Farbe Cyan");
              setColor(C_CYAN);
            }
            else if (header.indexOf("GET /color/gelb") >= 0) {
              Serial.println("Setze Farbe Gelb");
              setColor(C_GELB);
            }
            else if (header.indexOf("GET /color/lila") >= 0) {
              Serial.println("Setze Farbe Lila");
              setColor(C_LILA);
            }
            else if (header.indexOf("GET /color/orange") >= 0) {
              Serial.println("Setze Farbe Orange");
              setColor(C_ORANGE);
            }
            else if (header.indexOf("GET /color/pink") >= 0) {
              Serial.println("Setze Farbe Pink");
              setColor(C_PINK);
            }
            else if (header.indexOf("GET /color/rosa") >= 0) {
              Serial.println("Setze Farbe Rosa");
              setColor(C_ROSA);
            }
            else if(header.indexOf("GET /h") >= 0) 
            {
              int pos1 = header.indexOf('h');
              int pos2 = header.indexOf('&');
              String hueString = header.substring(pos1+2, pos2);
              /*Serial.println(redString.toInt());
              Serial.println(greenString.toInt());
              Serial.println(blueString.toInt());*/
              Serial.print("new COLOR: ");
              Serial.print(hueString);
              COLOR = hueString.toInt();
            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\", charset=\"UTF-8\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css\">");
            client.println("<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jscolor/2.0.4/jscolor.min.js\"></script>");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");
            
            
            
            // Web Page Heading
            client.println("<body><h1>Word Clock</h1>");
            
            client.println("<p><a href=\"/color/gruen\"><button class=\"button\">Grün</button></a></p>");
            client.println("<p><a href=\"/color/rot\"><button class=\"button\">Rot</button></a></p>");
            client.println("<p><a href=\"/color/blau\"><button class=\"button\">Blau</button></a></p>");
            client.println("<p><a href=\"/color/cyan\"><button class=\"button\">Cyan</button></a></p>");
            client.println("<p><a href=\"/color/gelb\"><button class=\"button\">Gelb</button></a></p>");
            client.println("<p><a href=\"/color/lila\"><button class=\"button\">Lila</button></a></p>");
            client.println("<p><a href=\"/color/orange\"><button class=\"button\">Orange</button></a></p>");
            client.println("<p><a href=\"/color/pink\"><button class=\"button\">Pink</button></a></p>");
            client.println("<p><a href=\"/color/rosa\"><button class=\"button\">Rosa</button></a></p>");
            client.println("<input type=\"range\" min=\"0\" max=\"65535\" value=\"0\" id=\"sliderRange\" name=\"Farbe\">");
            client.println("<p><a href=\"/h=0&\" id=\"hsv_link\"><button id=\"hsv_button\" class=\"button\">0</button></a></p>");
            client.println("<script>var rangeslider = document.getElementById(\"sliderRange\");var button = document.getElementById(\"hsv_button\");var link = document.getElementById(\"hsv_link\"); rangeslider.oninput = function() { link.href = \"/h=\" + this.value + \"&\"; button.innerHTML = this.value;}</script>");
            // The HTTP response ends with another blank line
            client.println();
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

void buttonClick()
{
  setColor((COLOR+1) % num_colors);
  Serial.println(COLOR);
}

void debugColors()
{
  for (int i = 0; i < NUM_PIXELS; i++)
  {
    Serial.print(pixel_array[i]);
    Serial.print(",");
  }
  Serial.println();
}

void setup()
{
  //Serial.begin(74880);
  Serial.begin(115200);
  struct	rst_info	*rtc_info	=	system_get_rst_info();
  Serial.print("reset	reason:	");
  Serial.println(rtc_info->reason);

	 if	(rtc_info->reason	==	REASON_WDT_RST	||

	 	 rtc_info->reason	==	REASON_EXCEPTION_RST	||

	 	 rtc_info->reason	==	REASON_SOFT_WDT_RST)	{

	 	 if	(rtc_info->reason	==	REASON_EXCEPTION_RST)	{

	 	 	 Serial.print("Fatal	exception	(%d):");
      Serial.println(rtc_info->exccause);

	 	 }

	 	 //os_printf("epc1=0x%08x,	epc2=0x%08x,	epc3=0x%08x,	excvaddr=0x%08x,depc=0x%08x\n",rtc_info->epc1,	rtc_info->epc2,	rtc_info->epc3,	rtc_info->excvaddr,	rtc_info->depc);
      //The	address	of	the	last	crash	is	printed,	which	is	used	to	debug	garbled	output.
	 }
  
  
  pixels.begin();

  button.attachClick(buttonClick);
  
  
  //try to connect to WIFI
  /*Serial.println("Setze WIFI LEDS");
  BRIGHTNESS = 255;
  COLOR = C_ROT;
  SATURATION = 255;
  _WIFI;
  pixels.show();
  delay(5000);
  Serial.println("Verbinde mit WIFI");
  while (!wifiConnect())
  {
    delay(2000);
  }
*/
  timeClient.begin();

  wifiManager.autoConnect("WORD CLOCK");
  server.begin();
  ArduinoOTA.setHostname("Word_Clock_OTA");
  ArduinoOTA.begin();

  EEPROM.begin(EEPROM_SIZE);
  initColor();
  
  Serial.println("Hallo OTA");
  timeClient.update();
  delay(2000);
  timeClient.update();
  updateBrightness(true);
  showTime();
  updateDisplay(false);
}

// the loop function runs over and over again forever
void loop()
{
  button.tick();
  ArduinoOTA.handle();
  wdt_reset();
  yield();
  timeClient.update();
  wdt_reset();
  yield();
  updateBrightness();
  yield();
  button.tick();
  pixels.clear();
  wdt_reset();
  yield();
  //Serial.println("before showTime:");
  //debugColors();
  showTime();
  //Serial.println("after showTime:");
  //debugColors();
  updateDisplay();
  //Serial.println("after updateDisplay:");
  //debugColors();
  //colorPicker();
  wdt_reset();
  yield();
  handleWebserver();
  
  
  
}