#pragma once

#include <cstddef>
#include <cstdint>

#include "INA3221/INA3221.h"
#include "driver/i2c_master.h"

struct Ina3221IdfI2c {
  i2c_master_bus_handle_t bus = nullptr;
  i2c_master_dev_handle_t dev = nullptr;
  uint8_t address = 0x40;
};

INA3221::Status ina3221IdfI2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                                   uint32_t timeoutMs, void* user);
INA3221::Status ina3221IdfI2cWriteRead(uint8_t addr, const uint8_t* txData,
                                       size_t txLen, uint8_t* rxData,
                                       size_t rxLen, uint32_t timeoutMs,
                                       void* user);
uint32_t ina3221IdfNowMs(void* user);
void ina3221IdfYield(void* user);
