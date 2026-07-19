/// @file Status.h
/// @brief Error codes and status handling for INA3221 driver
#pragma once

#include <cstdint>

namespace INA3221 {

/// @brief Error codes for all INA3221 operations.
enum class Err : uint8_t {
  OK = 0,                    ///< Operation successful
  NOT_INITIALIZED,           ///< begin() not called
  INVALID_CONFIG,            ///< Invalid configuration parameter
  I2C_ERROR,                 ///< I2C communication failure
  TIMEOUT,                   ///< Operation timed out
  INVALID_PARAM,             ///< Invalid parameter value
  DEVICE_NOT_FOUND,          ///< INA3221 not responding on I2C bus
  MANUFACTURER_ID_MISMATCH,  ///< Manufacturer ID != 0x5449
  DIE_ID_MISMATCH,           ///< Die ID != 0x3220
  CONVERSION_NOT_READY,      ///< Conversion not yet complete
  MEASUREMENT_NOT_READY = CONVERSION_NOT_READY, ///< Alias for cross-library uniformity
  BUSY,                      ///< Device is busy with conversion
  IN_PROGRESS,               ///< Operation scheduled; call tick() to complete
  I2C_NACK_ADDR,             ///< I2C address phase was not acknowledged
  I2C_NACK_DATA,             ///< I2C data phase was not acknowledged
  I2C_TIMEOUT,               ///< I2C transaction timed out
  I2C_BUS,                   ///< I2C bus or arbitration error
  JOB_BUSY,                  ///< A cooperative owner operation is active
  RESULT_PENDING,            ///< A terminal result must be taken first
  NO_RESULT,                 ///< No terminal result is pending
  CANCELLED,                 ///< Operation was cancelled without further I2C
  DEADLINE_EXPIRED,          ///< Caller-owned absolute deadline expired
  CONFIG_UNKNOWN,            ///< Required hardware configuration is unverified
  PROFILE_MISMATCH,          ///< Hardware readback does not match the profile
  READ_ONLY_REGISTER,        ///< Diagnostic write targeted a read-only register
  ARITHMETIC_OVERFLOW,       ///< Checked fixed-unit calculation overflowed
  OUT_OF_RANGE,              ///< Engineering-unit value is not representable
  NO_ACTIVE_JOB,             ///< No cooperative operation is active
  JOB_KIND_MISMATCH,         ///< Poll helper does not match the active job
  CONVERSION_BUSY,           ///< Legacy conversion state is active
  DEVICE_OFFLINE             ///< Passive diagnostic state value (never gates I2C)
};

/// @brief Status structure returned by all fallible operations.
struct Status {
  Err code = Err::OK;
  int32_t detail = 0;        ///< Implementation-specific detail (e.g., I2C error code)
  const char* msg = "";      ///< Static string describing the error

  constexpr Status() = default;
  constexpr Status(Err codeIn, int32_t detailIn, const char* msgIn)
      : code(codeIn), detail(detailIn), msg(msgIn) {}

  /// @return true if operation succeeded
  constexpr bool ok() const { return code == Err::OK; }

  /// @return true if operation in progress (not a failure)
  constexpr bool inProgress() const { return code == Err::IN_PROGRESS; }

  /// Create a success status
  static constexpr Status Ok() { return Status{Err::OK, 0, "OK"}; }

  /// Create an error status
  static constexpr Status Error(Err err, const char* message, int32_t detailCode = 0) {
    return Status{err, detailCode, message};
  }
};

} // namespace INA3221
