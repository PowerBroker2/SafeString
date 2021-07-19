#ifndef SAFE_STRING_NAMESPACE_END_H
#define SAFE_STRING_NAMESPACE_END_H

#if defined(ARDUINO_SAMD_ZERO) || defined(MEGATINYCORE_MAJOR)
// to skip this for SparkFun RedboardTurbo and MegaTinyCore
#else
#if defined(ARDUINO_ARDUINO_NANO33BLE) || defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_MEGAAVR)  || defined(ARDUINO_ARCH_MBED_RP2040) || defined(ARDUINO_ARCH_RP2040)
} // namespace arduino
#endif
#endif  // #if defined(ARDUINO_SAMD_ZERO) || defined(MEGATINYCORE_MAJOR)

#endif
