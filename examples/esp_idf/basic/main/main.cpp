#include <cstdint>

#include "INA3221/INA3221.h"
#include "Ina3221IdfI2cTransport.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char TAG[] = "ina3221_basic";
constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;
constexpr uint32_t I2C_FREQ_HZ = 400000;
constexpr uint8_t INA3221_ADDRESS = 0x40;

}  // namespace

extern "C" void app_main(void) {
  Ina3221IdfI2c transport{};
  transport.address = INA3221_ADDRESS;

  i2c_master_bus_config_t busConfig{};
  busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  busConfig.i2c_port = I2C_NUM_0;
  busConfig.sda_io_num = I2C_SDA;
  busConfig.scl_io_num = I2C_SCL;
  busConfig.glitch_ignore_cnt = 7;
  busConfig.flags.enable_internal_pullup = true;
  ESP_ERROR_CHECK(i2c_new_master_bus(&busConfig, &transport.bus));

  i2c_device_config_t devConfig{};
  devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  devConfig.device_address = INA3221_ADDRESS;
  devConfig.scl_speed_hz = I2C_FREQ_HZ;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(transport.bus, &devConfig, &transport.dev));

  INA3221::INA3221 device;
  INA3221::Config cfg{};
  cfg.i2cWrite = ina3221IdfI2cWrite;
  cfg.i2cWriteRead = ina3221IdfI2cWriteRead;
  cfg.i2cUser = &transport;
  cfg.nowMs = ina3221IdfNowMs;
  cfg.cooperativeYield = ina3221IdfYield;
  cfg.i2cAddress = INA3221_ADDRESS;
  cfg.i2cTimeoutMs = 50;
  cfg.mode = INA3221::Mode::SHUNT_BUS_CONT;
  cfg.shuntResistance[0] = 0.1f;
  cfg.shuntResistance[1] = 0.1f;
  cfg.shuntResistance[2] = 0.1f;

  INA3221::Status st = device.begin(cfg);
  if (!st.ok()) {
    ESP_LOGE(TAG, "begin failed: %s (%d detail=%ld)", st.msg, static_cast<int>(st.code),
             static_cast<long>(st.detail));
    return;
  }

  uint16_t manufacturer = 0;
  uint16_t dieId = 0;
  (void)device.readManufacturerId(manufacturer);
  (void)device.readDieId(dieId);
  ESP_LOGI(TAG, "manufacturer=0x%04X die=0x%04X", manufacturer, dieId);

  vTaskDelay(pdMS_TO_TICKS((device.getCycleTimeUs() + 999U) / 1000U + 1U));

  for (uint8_t i = 0; i < 3; ++i) {
    INA3221::ChannelMeasurement measurement{};
    st = device.readChannel(static_cast<INA3221::Channel>(i), measurement);
    if (st.ok()) {
      ESP_LOGI(TAG, "ch%u shunt=%.3f mV bus=%.3f V current=%.3f mA power=%.3f mW",
               static_cast<unsigned>(i + 1U),
               static_cast<double>(measurement.shuntVoltage_mV),
               static_cast<double>(measurement.busVoltage_V),
               static_cast<double>(measurement.current_mA),
               static_cast<double>(measurement.power_mW));
    } else {
      ESP_LOGW(TAG, "ch%u unavailable: %s (%d detail=%ld)", static_cast<unsigned>(i + 1U),
               st.msg, static_cast<int>(st.code), static_cast<long>(st.detail));
    }
  }

  ESP_LOGI(TAG, "state=%u successes=%lu failures=%lu", static_cast<unsigned>(device.state()),
           static_cast<unsigned long>(device.totalSuccess()),
           static_cast<unsigned long>(device.totalFailures()));
}
