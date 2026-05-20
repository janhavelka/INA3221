#include "Ina3221IdfI2cTransport.h"

#include <limits>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

Ina3221IdfI2c gTransport;

int clampTimeoutMs(uint32_t timeoutMs) {
  const uint32_t maxTimeout = static_cast<uint32_t>(std::numeric_limits<int>::max());
  return static_cast<int>(timeoutMs > maxTimeout ? maxTimeout : timeoutMs);
}

INA3221::Status mapEspErr(esp_err_t err, const char* context) {
  if (err == ESP_OK) {
    return INA3221::Status::Ok();
  }
  if (err == ESP_ERR_TIMEOUT) {
    return INA3221::Status::Error(INA3221::Err::I2C_TIMEOUT, "I2C timeout",
                                  static_cast<int32_t>(err));
  }
  if (err == ESP_ERR_INVALID_RESPONSE) {
    return INA3221::Status::Error(INA3221::Err::I2C_ERROR, "I2C invalid response/NACK",
                                  static_cast<int32_t>(err));
  }
  return INA3221::Status::Error(INA3221::Err::I2C_BUS, context, static_cast<int32_t>(err));
}

esp_err_t addDevice(Ina3221IdfI2c& ctx, uint8_t address, i2c_master_dev_handle_t& dev) {
  i2c_device_config_t devConfig{};
  devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  devConfig.device_address = address;
  devConfig.scl_speed_hz = ctx.frequencyHz;
  return i2c_master_bus_add_device(ctx.bus, &devConfig, &dev);
}

INA3221::Status validateContext(uint8_t addr, void* user, Ina3221IdfI2c*& ctx) {
  ctx = static_cast<Ina3221IdfI2c*>(user);
  if (ctx == nullptr || ctx->dev == nullptr) {
    return INA3221::Status::Error(INA3221::Err::I2C_BUS, "IDF I2C device not configured");
  }
  if (addr != ctx->address) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "Unexpected I2C address",
                                  static_cast<int32_t>(addr));
  }
  return INA3221::Status::Ok();
}

}  // namespace

Ina3221IdfI2c& ina3221IdfTransportContext() {
  return gTransport;
}

bool ina3221IdfInitI2c(int sda, int scl, uint32_t freqHz, uint16_t timeoutMs,
                       uint8_t address) {
  (void)timeoutMs;
  ina3221IdfDeinitI2c();

  i2c_master_bus_config_t busConfig{};
  busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  busConfig.i2c_port = I2C_NUM_0;
  busConfig.sda_io_num = static_cast<gpio_num_t>(sda);
  busConfig.scl_io_num = static_cast<gpio_num_t>(scl);
  busConfig.glitch_ignore_cnt = 7;
  busConfig.flags.enable_internal_pullup = true;

  esp_err_t err = i2c_new_master_bus(&busConfig, &gTransport.bus);
  if (err != ESP_OK) {
    gTransport.lastError = err;
    return false;
  }

  gTransport.frequencyHz = freqHz;
  gTransport.address = address;
  err = addDevice(gTransport, address, gTransport.dev);
  if (err != ESP_OK) {
    (void)i2c_del_master_bus(gTransport.bus);
    gTransport.bus = nullptr;
    gTransport.lastError = err;
    return false;
  }

  gTransport.lastError = ESP_OK;
  return true;
}

void ina3221IdfDeinitI2c() {
  if (gTransport.dev != nullptr) {
    (void)i2c_master_bus_rm_device(gTransport.dev);
    gTransport.dev = nullptr;
  }
  if (gTransport.bus != nullptr) {
    (void)i2c_del_master_bus(gTransport.bus);
    gTransport.bus = nullptr;
  }
}

INA3221::Status ina3221IdfProbeAddress(uint8_t address, uint16_t timeoutMs) {
  if (gTransport.bus == nullptr) {
    return INA3221::Status::Error(INA3221::Err::I2C_BUS, "IDF I2C bus not configured");
  }
  gTransport.lastError = i2c_master_probe(gTransport.bus, address, timeoutMs);
  return mapEspErr(gTransport.lastError, "I2C address probe failed");
}

esp_err_t ina3221IdfLastError() {
  return gTransport.lastError;
}

INA3221::Status ina3221IdfI2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                                   uint32_t timeoutMs, void* user) {
  Ina3221IdfI2c* ctx = nullptr;
  INA3221::Status st = validateContext(addr, user, ctx);
  if (!st.ok()) {
    return st;
  }
  if (data == nullptr || len == 0) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "Invalid I2C write buffer");
  }

  ctx->lastError = i2c_master_transmit(ctx->dev, data, len, clampTimeoutMs(timeoutMs));
  return mapEspErr(ctx->lastError, "I2C write failed");
}

INA3221::Status ina3221IdfI2cWriteRead(uint8_t addr, const uint8_t* txData,
                                       size_t txLen, uint8_t* rxData,
                                       size_t rxLen, uint32_t timeoutMs,
                                       void* user) {
  Ina3221IdfI2c* ctx = nullptr;
  INA3221::Status st = validateContext(addr, user, ctx);
  if (!st.ok()) {
    return st;
  }
  if (txData == nullptr || txLen == 0 || (rxLen > 0 && rxData == nullptr)) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "Invalid I2C read buffer");
  }

  ctx->lastError = i2c_master_transmit_receive(
      ctx->dev, txData, txLen, rxData, rxLen, clampTimeoutMs(timeoutMs));
  return mapEspErr(ctx->lastError, "I2C write-read failed");
}

INA3221::Status ina3221IdfI2cWriteReadAt(uint8_t addr, const uint8_t* txData,
                                         size_t txLen, uint8_t* rxData,
                                         size_t rxLen, uint32_t timeoutMs,
                                         void* user) {
  Ina3221IdfI2c* ctx = static_cast<Ina3221IdfI2c*>(user);
  if (ctx == nullptr || ctx->bus == nullptr) {
    return INA3221::Status::Error(INA3221::Err::I2C_BUS, "IDF I2C bus not configured");
  }
  if (txData == nullptr || txLen == 0 || rxData == nullptr || rxLen == 0) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "Invalid I2C read buffer");
  }

  i2c_master_dev_handle_t tempDev = nullptr;
  i2c_master_dev_handle_t dev = ctx->dev;
  if (addr != ctx->address) {
    ctx->lastError = addDevice(*ctx, addr, tempDev);
    if (ctx->lastError != ESP_OK) {
      return mapEspErr(ctx->lastError, "I2C temporary device failed");
    }
    dev = tempDev;
  }
  if (dev == nullptr) {
    return INA3221::Status::Error(INA3221::Err::I2C_BUS, "IDF I2C device not configured");
  }

  ctx->lastError = i2c_master_transmit_receive(
      dev, txData, txLen, rxData, rxLen, clampTimeoutMs(timeoutMs));
  if (tempDev != nullptr) {
    (void)i2c_master_bus_rm_device(tempDev);
  }
  return mapEspErr(ctx->lastError, "I2C write-read failed");
}

uint32_t ina3221IdfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void ina3221IdfYield(void*) {
  taskYIELD();
}
