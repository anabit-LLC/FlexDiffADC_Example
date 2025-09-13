/********************************************************************************************************
This example Arduino sketch is meant to work with Anabit's Flex Differential ADC open source reference design and will run on any Arduino that supports
Hardware SPI communication

Product link: https://anabit.co/products/flex-differential-adc

The Flex Differential ADC design uses a pseudo differential ADC. Why is it pseudo differential? It is pseudo differential because it does not support negative 
voltages, refernced to the ADCs ground, and its common mode voltage is always vref/2. It can only support negative voltage below the common mode voltage. 
This ADC does provide 14 bits of resolution for both the +in range (0 to vref) and -in range (0 to vref). The ADC returns the difference between +in input and 
the -in input in the form of a 15 bit value. If +in is larger in voltage than -in, the 15th bit will be zero and the range is 0V --> 0 code to VREF --> 16383 code. 
If -in is larger in voltage than +in the 15th bit (MSB) will be one and the range is 0V --> 16383 code to -2.048 --> 0 code (with 15th bit set to 0). The counting 
of the code is reverse when -in is larger. That means if you are measuring a value where +in and -in are almost equal the codes can jumpr from slightly higher 
than 0 (+in is slightly larger) and slightly lower than 16383 (-in is larger). You can use this ADC as a single ended input by connecting one of the inputs to 
ground and using the other to measure the input voltage. It is more intutive to use +in as the input since there will be no 15th bit and the 14 bit code will 
count up with the voltage.

This sketch deomonstrats how to use the Flex ADC to make a single measurement or to make a group or burst of measurements as fast as possible. The single versus
burst mode is set by the "#define" MODE_SINGLE_MEASUREMENT or MODE_BURST_CAPTURE, comment out the mode you don't want to use

Please report any issue with the sketch to the Anagit forum: https://anabit.co/community/forum/analog-to-digital-converters-adcs
Example code developed by Your Anabit LLC © 2025
Licensed under the Apache License, Version 2.0.
*/

#include <SPI.h>

// ===================== CONFIGURATION =====================
// Select ONE mode:
#define MODE_SINGLE_MEASUREMENT
//#define MODE_BURST_CAPTURE

// Chip-select pin
#define CS_PIN 10

// SPI clock for the ADC (reduce if your board/ADC wiring can't sustain this)
#define ADS_SPI_HZ 40000000UL  // Arduino boards that cannot support 40MHz clock rate will automatically scale down clock freq

// ADC/reference constants
constexpr float VREF_VOLTAGE = 4.096f;   // ADC reference voltage in volts
constexpr float ADC_COUNTS   = 16383.0f; // 14-bit code range (0..16383)

// SPI mode/settings for the ADC (mode 0, MSB first)
SPISettings adsSettings(ADS_SPI_HZ, MSBFIRST, SPI_MODE0);

// ===== Burst mode buffers =====
#if defined(MODE_BURST_CAPTURE)
const int NUM_SAMPLES = 256;
uint16_t adcRaw[NUM_SAMPLES];
float    adcVoltage[NUM_SAMPLES];
#endif
// ==========================================================

void setup() {
  Serial.begin(115200);
  delay(2500); // time to open Serial Monitor/Plotter

  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  SPI.begin();

#if defined(MODE_SINGLE_MEASUREMENT)
  Serial.println(F("Single Measurement Mode (CH0 via GPIO)"));
  readADS7945(0xC000);            // pipeline dummy read
  delayMicroseconds(5);

  while (true) {
    uint16_t raw = readADS7945(0xC000)>>1;
    Serial.print(F("ADC Value unsigned hex: "));     Serial.println(raw, HEX);
    Serial.print(F("ADC Value unsigned decimal: ")); Serial.println(raw);
    float voltage = convertToVoltage(raw);
    Serial.print(F("Voltage: ")); Serial.print(voltage, 4); Serial.println(F(" V"));
    Serial.println();
    delay(2500);
  }

#elif defined(MODE_BURST_CAPTURE)
  Serial.println(F("Burst Capture Mode (CH0 via GPIO)"));
  while (true) {
    captureBurstPortable();
    for (int i = 0; i < NUM_SAMPLES; i++) {
      Serial.println(adcVoltage[i], 4);  // suitable for Serial Plotter
    }
    delay(2000);
  }
#else
  #error "Please define one of the modes."
#endif
}

void loop() {
  // not used
}

// ===== Helpers =====

// One 16-bit framed transaction to the ADC
uint16_t readADS7945(uint16_t cmd) {
  SPI.beginTransaction(adsSettings);
  digitalWrite(CS_PIN, LOW);
  uint16_t value = SPI.transfer16(cmd);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
  return value;
}

// Convert raw 14-bit code to voltage (handles sign per original scheme)
float convertToVoltage(uint16_t raw_code) {
  // If bit14 is set, treat as negative magnitude encoded above 0x3FFF
  if (raw_code > 0x3FFF) {
    uint16_t mag = (raw_code & 0x3FFF);
    mag = 0x3FFF - mag;  // reverse count direction for negative half
    return - ( (float)mag / ADC_COUNTS ) * VREF_VOLTAGE;
  } else {
    return ( (float)raw_code / ADC_COUNTS ) * VREF_VOLTAGE;
  }
}

#if defined(MODE_BURST_CAPTURE)
// Portable burst capture: standard digitalWrite + SPI
void captureBurstPortable() {
  // Prime the pipeline
  readADS7945(0xC000);
  delayMicroseconds(5);

  noInterrupts();
  SPI.beginTransaction(adsSettings);

  uint32_t t0 = micros();
  for (int i = 0; i < NUM_SAMPLES; i++) {
    digitalWrite(CS_PIN, LOW);
    adcRaw[i] = SPI.transfer16(0xC000);
    digitalWrite(CS_PIN, HIGH);
  }
  uint32_t duration = micros() - t0;

  SPI.endTransaction();
  interrupts();

  Serial.print(F("Operation took "));
  Serial.print(duration);
  Serial.println(F(" microseconds."));

  for (int i = 0; i < NUM_SAMPLES; i++) {
    adcVoltage[i] = convertToVoltage((adcRaw[i]>>1));
  }
}
#endif
