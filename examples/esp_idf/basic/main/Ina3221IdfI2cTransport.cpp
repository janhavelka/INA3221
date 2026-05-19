#include "Ina3221IdfI2cTransport.h"

#include <limits>

#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

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

INA3221::Status validateContext(uint8_t addr, const void* user, const Ina3221IdfI2c*& ctx) {
  ctx = static_cast<const Ina3221IdfI2c*>(user);
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

INA3221::Status ina3221IdfI2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                                   uint32_t timeoutMs, void* user) {
  const Ina3221IdfI2c* ctx = nullptr;
  INA3221::Status st = validateContext(addr, user, ctx);
  if (!st.ok()) {
    return st;
  }
  if (data == nullptr || len == 0) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "Invalid I2C write buffer");
  }

  const esp_err_t err =
      i2c_master_transmit(ctx->dev, data, len, clampTimeoutMs(timeoutMs));
  return mapEspErr(err, "I2C write failed");
}

INA3221::Status ina3221IdfI2cWriteRead(uint8_t addr, const uint8_t* txData,
                                       size_t txLen, uint8_t* rxData,
                                       size_t rxLen, uint32_t timeoutMs,
                                       void* user) {
  const Ina3221IdfI2c* ctx = nullptr;
  INA3221::Status st = validateContext(addr, user, ctx);
  if (!st.ok()) {
    return st;
  }
  if (txData == nullptr || txLen == 0 || (rxLen > 0 && rxData == nullptr)) {
    return INA3221::Status::Error(INA3221::Err::INVALID_PARAM, "Invalid I2C read buffer");
  }

  const esp_err_t err = i2c_master_transmit_receive(
      ctx->dev, txData, txLen, rxData, rxLen, clampTimeoutMs(timeoutMs));
  return mapEspErr(err, "I2C write-read failed");
}

uint32_t ina3221IdfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void ina3221IdfYield(void*) {
  taskYIELD();
}
