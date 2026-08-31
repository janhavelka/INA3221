/// @file Status.h
/// @brief Framework-neutral error codes and status values for the INA3221 driver.
#pragma once

#include <cstdint>

namespace INA3221 {

/// @brief Error codes for all INA3221 operations.
enum class Err : uint8_t {
  OK = 0,                    ///< Operation successful
  NOT_INITIALIZED,           ///< Required binding or initialization is absent
  INVALID_CONFIG,            ///< Invalid configuration parameter
  I2C_ERROR,                 ///< Transport-supplied generic I2C communication failure
  TIMEOUT,                   ///< Legacy/transport-supplied generic timeout category
  INVALID_PARAM,             ///< Invalid parameter value
  DEVICE_NOT_FOUND,          ///< INA3221 not responding on I2C bus
  MANUFACTURER_ID_MISMATCH,  ///< Manufacturer ID != 0x5449
  DIE_ID_MISMATCH,           ///< Die ID != 0x3220
  CONVERSION_NOT_READY,      ///< Conversion not yet complete
  MEASUREMENT_NOT_READY = CONVERSION_NOT_READY, ///< Alias of produced CONVERSION_NOT_READY value
  BUSY,                      ///< Reserved legacy value; not produced by this library
  IN_PROGRESS,               ///< Cooperative operation admitted; continue polling
  I2C_NACK_ADDR,             ///< Transport-supplied address-phase NACK
  I2C_NACK_DATA,             ///< Transport-supplied data-phase NACK
  I2C_TIMEOUT,               ///< Transport-supplied I2C transaction timeout
  I2C_BUS,                   ///< Transport-supplied bus or arbitration error
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
  DEVICE_OFFLINE             ///< Reserved legacy value; passive OFFLINE is exposed through DriverState
};

/// @brief Status structure returned by all fallible operations.
struct Status {
  Err code = Err::OK;          ///< Stable library error category.
  int32_t detail = 0;        ///< Implementation-specific detail (e.g., I2C error code)
  const char* msg = "";      ///< Static string describing the error

  /// @brief Construct a successful default status.
  constexpr Status() = default;

  /// @brief Construct a status from its three public fields.
  /// @param codeIn Stable library error category.
  /// @param detailIn Backend-specific or operation-specific diagnostic value.
  /// @param msgIn Pointer to a static-lifetime diagnostic string.
  constexpr Status(Err codeIn, int32_t detailIn, const char* msgIn)
      : code(codeIn), detail(detailIn), msg(msgIn) {}

  /// @return `true` when code is Err::OK.
  constexpr bool ok() const { return code == Err::OK; }

  /// @return `true` when a cooperative operation was admitted and remains active.
  constexpr bool inProgress() const { return code == Err::IN_PROGRESS; }

  /// @brief Create a canonical success status.
  /// @return Status with Err::OK, detail zero, and a static `"OK"` message.
  static constexpr Status Ok() { return Status{Err::OK, 0, "OK"}; }

  /// @brief Create an error status without allocating memory.
  /// @param err Stable library error category.
  /// @param message Pointer to a static-lifetime diagnostic string.
  /// @param detailCode Optional backend-specific or operation-specific detail.
  /// @return Status containing the supplied values.
  static constexpr Status Error(Err err, const char* message, int32_t detailCode = 0) {
    return Status{err, detailCode, message};
  }
};

} // namespace INA3221
