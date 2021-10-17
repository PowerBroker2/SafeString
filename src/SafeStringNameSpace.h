#ifndef SAFE_STRING_NAMESPACE_H
#define SAFE_STRING_NAMESPACE_H
//#include "SafeStringNameSpace.h"

#if defined(ARDUINO_SAMD_ZERO) || defined(MEGATINYCORE_MAJOR) || defined(MOTEINO_M0) || defined(RFGATEWAY_M4) || defined(CURRENT_RANGER) || defined(__SAMD51__)
// to skip this for SparkFun RedboardTurbo and MegaTinyCore and MOTEINO boards and Adafruit M4 boards
#else
#if defined(ARDUINO_ARDUINO_NANO33BLE) || defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_MEGAAVR) || defined(ARDUINO_ARCH_MBED_RP2040) || defined(ARDUINO_ARCH_RP2040)
using namespace arduino;
#endif
#endif // #if defined(ARDUINO_SAMD_ZERO) || defined(MEGATINYCORE_MAJOR)

#endif