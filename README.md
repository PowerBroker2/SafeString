# SafeString
This SafeString library is designed for beginners to be a safe, robust and debuggable replacement for string processing in Arduino and provides non-blocking text I/O and parsing and testing for Real World use.

This library includes:-  
* **SafeString**, a safe, robust and debuggable replacement for string processing in Arduino  
* **SafeStringReader**, a non-blocking tokenizing text reader replacement for Serial read()  
* **BufferedOutput**, non-blocking replacement for Serial print()  
* **SafeStringStream**, a stream to provide test inputs for repeated testing of I/O sketches   
* **BufferedInput**, extra buffering for text input  
* **loopTimer**, to track of the maximum and average run times for the loop()  
* **millisDelay**, a non-blocking delay replacement, with single-shot, repeating, restart and stop facilities.  

# How-To
See [SafeString Tutorial](https://www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html).  
See [Arduino Text I/O for the Real World](https://www.forward.com.au/pfod/ArduinoProgramming/Serial_IO/index.html).  
See [Simple Multitasking Arduino](https://www.forward.com.au/pfod/ArduinoProgramming/RealTimeArduino/index.html)  
See [How to code Timers and Delays in Arduino](https://www.forward.com.au/pfod/ArduinoProgramming/TimingDelaysInArduino.html)  

# Software License
See the top of each file for its license

# Note
Note, this is NOT my work, I am simply hosting it for easy access. The original code belongs to [Forward Computing and Control Pty. Ltd](https://www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html).

# Revisions
V3.1.0 adds hasError() method
V3.0.6 adds support for Arduino megaAVR boards
V3.0.5 adds support for SparkFun Redboard Turbo,but may interfer with other SAM ZERO based boards, also adds support for Due and STM32F1 and STM32F4

