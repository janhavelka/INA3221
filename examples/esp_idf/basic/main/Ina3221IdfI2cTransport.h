#pragma once

#include <cstddef>
#include <cstdint>

#include "INA3221/INA3221.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

struct Ina3221IdfI2c {
  i2c_master_bus_handle_t bus = nullptr;
  i2c_master_dev_handle_t dev = nullptr;
  uint8_t address = 0x40;
  uint32_t frequencyHz = 400000;
  esp_err_t lastError = ESP_OK;
};

Ina3221IdfI2c& ina3221IdfTransportContext();
bool ina3221IdfInitI2c(int sda, int scl, uint32_t freqHz, uint16_t timeoutMs,
                       uint8_t address);
void ina3221IdfDeinitI2c();
INA3221::Status ina3221IdfProbeAddress(uint8_t address, uint16_t timeoutMs);
esp_err_t ina3221IdfLastError();

INA3221::Status ina3221IdfI2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                                   uint32_t timeoutMs, void* user);
INA3221::Status ina3221IdfI2cWriteRead(uint8_t addr, const uint8_t* txData,
                                       size_t txLen, uint8_t* rxData,
                                       size_t rxLen, uint32_t timeoutMs,
                                       void* user);
INA3221::Status ina3221IdfI2cWriteReadAt(uint8_t addr, const uint8_t* txData,
                                         size_t txLen, uint8_t* rxData,
                                         size_t rxLen, uint32_t timeoutMs,
                                         void* user);
uint32_t ina3221IdfNowMs(void* user);
void ina3221IdfYield(void* user);
