/*
   PinFlasher_example.h   for UNO
   by Matthew Ford,  2025/04/04
   (c)2025 Forward Computing and Control Pty. Ltd.
   NSW, Australia  www.forward.com.au
   This code may be freely used for both private and commerical use.
   Provide this copyright is maintained.
*/

#include "PinFlasher.h"

const int LED_PIN = 13; // << set this to match your board

PinFlasher flasher(LED_PIN); // high for LED on
//PinFlasher flasher(LED_PIN,true); // to invert logic to LOW for LED on

void setup() {
  //flasher.setOnOff(PIN_ON); // turn led on,  NOTE: if led turns OFF instead, use PinFlasher flasher(LED_PIN,true); 
  //flasher.setOnOff(PIN_OFF); // turn led off, NOTE: if led turns ON instead, use PinFlasher flasher(LED_PIN,true); 
  //flasher.setOnOff(1000); // flash on/off 1sec each for period of 2sec
  
  flasher.setOnAndOff(200,1800); // flash on for 200ms and off for 1800ms for period of 2sec
  // if flash is on for 1800ms, use PinFlasher flasher(LED_PIN,true); 
}

void loop() {
  flasher.update(); // need to call this each loop the toggle led state based on timings set
}
