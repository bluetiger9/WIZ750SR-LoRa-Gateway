#ifndef SERIAL_TO_ETHERNET_LORA_H_
#define SERIAL_TO_ETHERNET_LORA_H_

// Copyright (c) Sandeep Mistry. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef LORA_H
#define LORA_H

#include <stdint.h>
#include <stddef.h>

#include "W7500x_gpio.h"
#include "W7500x_ssp.h"

//#define LORA_DEFAULT_SPI           SPI
//#define LORA_DEFAULT_SPI_FREQUENCY 8E6
#define LORA_DEFAULT_SS_PIN        GPIO_Pin_13
#define LORA_DEFAULT_RESET_PIN     GPIO_Pin_12
#define LORA_DEFAULT_DIO0_PIN      GPIO_Pin_9

#define PA_OUTPUT_RFO_PIN          0
#define PA_OUTPUT_PA_BOOST_PIN     1

class LoRaClass {
public:
  LoRaClass();

  int begin(long frequency);
  void end();

  int beginPacket(int implicitHeader = false);
  int endPacket();

  int parsePacket(int size = 0);
  int packetRssi();
  float packetSnr();
  long packetFrequencyError();

  // from Print
  size_t write(uint8_t byte);
  size_t write(const uint8_t *buffer, size_t size);

  // from Stream
  int available();
  int read();
  int peek();

#ifndef ARDUINO_SAMD_MKRWAN1300
  void onReceive(void(*callback)(int));

  void receive(int size = 0);
#endif

  void idle();
  void sleep();

  void setTxPower(int level, int outputPin = PA_OUTPUT_PA_BOOST_PIN);
  void setFrequency(long frequency);
  void setSpreadingFactor(int sf);
  void setSignalBandwidth(long sbw);
  void setCodingRate4(int denominator);
  void setPreambleLength(long length);
  void setSyncWord(int sw);
  void enableCrc();
  void disableCrc();

  // deprecated
  void crc() { enableCrc(); }
  void noCrc() { disableCrc(); }

  uint8_t random();

  void setPins(uint32_t ss = LORA_DEFAULT_SS_PIN, uint32_t reset = LORA_DEFAULT_RESET_PIN, uint32_t dio0 = LORA_DEFAULT_DIO0_PIN);
  //void setSPI(SPIClass& spi);
  //void setSPIFrequency(uint32_t frequency);

  void dumpRegisters();

private:
  void explicitHeaderMode();
  void implicitHeaderMode();

  void handleDio0Rise();

  int getSpreadingFactor();
  long getSignalBandwidth();

  void setLdoFlag();

  uint8_t readRegister(uint8_t address);
  void writeRegister(uint8_t address, uint8_t value);
  uint8_t singleTransfer(uint8_t address, uint8_t value);

  static void onDio0Rise();

private:
  //SPISettings _spiSettings;
  //SPIClass* _spi;
  uint32_t _ss;
  uint32_t _reset;
  uint32_t _dio0;
  long _frequency;
  int _packetIndex;
  int _implicitHeaderMode;
  void (*_onReceive)(int);
};

extern LoRaClass LoRa;

#endif

#endif /* SERIAL_TO_ETHERNET_LORA_H_ */
