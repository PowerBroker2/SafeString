# SafeString
This SafeString library is designed for beginners to be a safe, robust and debuggable replacement for string processing in Arduino and provides non-blocking text I/O and parsing and testing for Real World use.

This library includes:-  
* **SafeString**, a safe, robust and debuggable replacement for string processing in Arduino  
* **SafeStringReader**, a non-blocking tokenizing text reader replacement for Serial read()  
* **BufferedOutput**, non-blocking replacement for Serial print()  
* **SafeStringStream**, a stream to provide test inputs for repeated testing of I/O sketches   
* **BufferedInput**, extra buffering for text input  
* **loopTimer**, (loopTimerClass) to track of the maximum and average run times for the loop()  
* **millisDelay**, a non-blocking delay replacement, with single-shot, repeating, restart and stop facilities.  
* **PinFlasher**, a non-blocking flashing of an output pin.  
* **SerialComs**, to send messages between Arduinos via Serial

# How-To
See [SafeString Tutorial](https://www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html)  
See [Arduino to Arduino/PC via Serial](https://www.forward.com.au/pfod/ArduinoProgramming/SoftwareSolutions/ComsPair.html)  
See [Arduino Text I/O for the Real World](https://www.forward.com.au/pfod/ArduinoProgramming/Serial_IO/index.html)  
See [Simple Multitasking Arduino](https://www.forward.com.au/pfod/ArduinoProgramming/RealTimeArduino/index.html)  
See [How to code Timers and Delays in Arduino](https://www.forward.com.au/pfod/ArduinoProgramming/TimingDelaysInArduino.html)  

# Software License
See the top of each file for its license

# Note
Note, this is NOT my work, I am simply hosting it for easy access. The original code belongs to [Forward Computing and Control Pty. Ltd](https://www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html).

# Revisions
V4.1.14 minor fix PinFlasher  
V4.1.13 minor change PinFlasher to simplify loop logic    
V4.1.12 adds PinFlasher class  
V4.1.11 adds support for Ardafruit M4 and Moteino M4  
V4.1.10 fixed SafeStringStream to handle data >= 0x80  
V4.1.9 added toUnsignedLong coversions, removed firstToken()  
V4.1.7-8 added MegaTinyCore support 
V4.1.6 added Teensy2 to Teensy4.1 support (dtostrf)  
V4.1.5 SerialComs timeout 5sec, added firstToken()  
V4.1.4 fixed dtostrf support  
V4.1.3 added Arduino Due support  
V4.1.2 fixed SerialComs when msg times out without delimiter, fixed support for Adafruit Feather nRF52 (V0.21.0)  
V4.1.1 fixed nullpointer, check for Out-Of-Memory on createSafeString, support for Earl Philhower's pi pico board package  
V4.1.0 added SerialComs class for Arduino to Arduino/PC via Serial, added fixed width formatting, print(value,decPlaces,width) see example SafeString_Tests/SafeString_fixedWidthFormat.ino  
V4.0.5 added returnEmptyTokens() option to SafeStringReader  
V4.0.4 adds support for Raspberry Pi Pico using Arduino Mbed OS RP2040 V2.0.0 board package ,nextToken() now returns last un-terminated token by default (can be overridded by optional arg), option to return empty tokens  
V4.0.3 allow createSafeString for small sizes, fixed bool for DUE (ARDUINO_ARCH_SAM)  
V4.0.2 added flushInput() method to SafeStringReader  
V4.0.1 fixed SafeStringReader timeout and NanoBLE F() macro  
V4.0.0 changes method returns to better match Arduino String methods, main change is indexOf now returns int and returns -1 if not found  
V3.1.0 adds hasError() method  
V3.0.6 adds support for Arduino megaAVR boards  
V3.0.5 adds support for SparkFun Redboard Turbo,but may interfer with other SAM ZERO based boards, also adds support for Due and STM32F1 and STM32F4  

