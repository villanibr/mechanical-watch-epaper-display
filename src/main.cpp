// *****************************************************************************
// Code for a mechanically powered and controlled watch epaper display prototype.
// Current components:
// - ESP32 Wemos Lolin32 Lite (circuit logic)
// - HINK-E154A07-A1 epaper display and board (to show 24hh:mi time)
// - Generic 3v3 power source
// - Commodity components
// Author: Leandro Casella
// Start: 2023-05-05
// References / code snippet sources (credits):
// https://www.circuitschools.com/interfacing-16x2-lcd-module-with-esp32-with-and-without-i2c/
// https://lastminuteengineers.com/esp32-deep-sleep-wakeup-sources/
// https://randomnerdtutorials.com/esp32-external-wake-up-deep-sleep/
// https://github.com/ZinggJM/GxEPD2/tree/master
// *****************************************************************************

#include <Arduino.h>
#include <string>
#include "driver/gpio.h"
#include "driver/rtc_io.h"

// For LCD displays
#include <LiquidCrystal.h>

// For epaper displays
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

// select the display class and display driver class in the following file (new style):
#include "GxEPD2_display_selection_new_style.h"


using namespace std;

// Mask for GPIO#32 and GPIO#33 pins, that will wake up ESP32 from deep sleep
// GPIO#32: minute increment
// GPIO#33: reset to zero minutes
#define BUTTON_PIN_BITMASK 0x300000000

const int minuteCountStart = ((23 * 60) + 58) - 1;

// Values stored even in deep sleep
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int minuteCount = minuteCountStart;

// initialize the LCD library with the numbers of the interface pins
LiquidCrystal lcd(19, 23, 18, 17, 16, 15);

/// @brief Method to print the reason by which ESP32 has been awaken from sleep
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

/// @brief Returns the decimal number of the pin that woke the board
/// @return 
int get_ext1_wakeup_pin(){
  uint64_t wakeup_pin_base2 = esp_sleep_get_ext1_wakeup_status();
  return log(wakeup_pin_base2)/log(2);
}

/// @brief Format string with arguments, according to standard printf format masks
/// @param fmt 
/// @param  
/// @return 
std::string string_format(const std::string fmt, ...) {
    int size = ((int)fmt.size()) * 2 + 50;   // Use a rubric appropriate for your code
    std::string str;
    va_list ap;
    while (1) {     // Maximum two passes on a POSIX system...
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.data(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size) {  // Everything worked
            str.resize(n);
            return str;
        }
        if (n > -1)  // Needed size returned
            size = n + 1;   // For null char
        else
            size *= 2;      // Guess at a larger size (OS specific)
    }
    return str;
}

const char HelloWorld[] = "Hello World!";


void setup() {
  
  Serial.begin(115200);
  //delay(1000); // Take some time to open up the Serial Monitor


  // **********
  // Wakeup
  // **********

  /*
  First we configure the wake up source
  We set our ESP32 to wake up for an external trigger.
  There are two types for ESP32, ext0 and ext1 .
  ext0 uses RTC_IO to wakeup thus requires RTC peripherals
  to be on while ext1 uses RTC Controller so doesnt need
  peripherals to be powered on.
  Note that using internal pullups/pulldowns also requires
  RTC peripherals to be turned on.
  */
  //esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,1); //1 = High, 0 = Low

  //If you were to use ext1, you would use it like
  esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);

  // Print the wakeup reason for ESP32
  print_wakeup_reason();

  // Get the pin that woke the board and reset the minute counter, depending on the pin
  int wakeup_pin = get_ext1_wakeup_pin();
  Serial.println("Wakeup pin: " + String(wakeup_pin));
  if(wakeup_pin == GPIO_NUM_33)
    minuteCount = -1;


  // **********
  // Counters
  // **********

  // Increment boot number and print it every reboot
  ++bootCount;
  String bootNumberMessage = "Boot number: " + String(bootCount);
  Serial.println(bootNumberMessage);

  // Increment minute counter
  const int minMinutes = 0;
  const int maxMinutes = 24 * 60;
  ++minuteCount;
  if(minuteCount >= maxMinutes)
    minuteCount = minMinutes;

  // Format time for display (hh24:mi) and print it
  int hours = minuteCount / 60;
  int minutes = minuteCount % 60;
  //String time = String(hours) + ":" + String(minutes);
  std:string formattedTimeCpp = string_format("%02d:%02d", hours, minutes);
  String formattedTime = String(formattedTimeCpp.c_str());
  String timeMessage = "Time: " + formattedTime;
  Serial.println(timeMessage);
  

  // **********
  // LCD
  // **********

  /*// set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print(bootNumberMessage);

  // Print the time
  lcd.setCursor(0, 1);
  lcd.print(timeMessage);*/


  // **********
  // Epaper
  // **********
  //display.init(115200); // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
  display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
    
  //display.setRotation(1);
  display.setRotation(3);
  //display.setFont(&FreeMonoBold9pt7b);
  display.setFont(&FreeMonoBold18pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(formattedTime, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(formattedTime);
  }
  while (display.nextPage());
    
  display.hibernate();


  // **********
  // Sleep
  // **********

  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/sleep_modes.html
  // Not tested to check if power consumption decreases or increases
  //rtc_gpio_isolate(GPIO_NUM_32);
  //rtc_gpio_isolate(GPIO_NUM_33);

  // Go to sleep now
  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}

void loop() {
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  //lcd.setCursor(0, 1);
  // print the number of seconds since reset:
  //lcd.print(millis() / 1000);
}
