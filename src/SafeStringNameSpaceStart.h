#ifndef SAFE_STRING_NAMESPACE_START_H
#define SAFE_STRING_NAMESPACE_START_H

// to skip this for SparkFun RedboardTurbo
#ifndef ARDUINO_SAMD_ZERO
#if defined(ARDUINO_ARDUINO_NANO33BLE) || defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_MEGAAVR) || defined(ARDUINO_ARCH_MBED_RP2040) || defined(ARDUINO_ARCH_RP2040)
namespace arduino {
#endif
#endif // #ifndef ARDUINO_SAMD_ZERO


#endif