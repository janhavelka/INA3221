/// @file ScriptedTransport.h
/// @brief Fixed-capacity deterministic INA3221 transport for native tests.
#pragma once

#include <cstddef>
#include <cstdint>

#include "INA3221/CommandTable.h"
#include "INA3221/Config.h"

namespace INA3221Test {

/// Strict, allocation-free transport fake for stage and fault-injection tests.
///
/// Every callback must match the next queued expectation. The fake checks the
/// configured address and timeout as well as the INA3221 register transaction
/// shape (three-byte writes and one-byte-pointer/two-byte reads).
class ScriptedTransport {
public:
  static constexpr size_t MAX_SCRIPT_STEPS = 96;
  static constexpr size_t MAX_CALLS = 128;

  enum class TransferKind : uint8_t {
    WRITE,
    WRITE_READ
  };

  /// Hardware effect of a write whose callback returns an error.
  enum class WriteEffect : uint8_t {
    ON_SUCCESS,           ///< Failed callback does not change the register.
    COMMIT_BEFORE_STATUS  ///< Register changes before the error is returned.
  };

  /// Data/effect to model when a read callback returns an error.
  enum class ReadErrorEffect : uint8_t {
    NONE,             ///< No data and no destructive register-read effect.
    FULL_DATA,        ///< Both bytes were received before the error.
    FIRST_BYTE_ONLY   ///< Only rx[0] was received before the error.
  };

  enum class Violation : uint8_t {
    NONE,
    SCRIPT_CAPACITY,
    CALL_CAPACITY,
    UNEXPECTED_CALL,
    WRONG_KIND,
    WRONG_ADDRESS,
    WRONG_TIMEOUT,
    NULL_BUFFER,
    WRONG_WRITE_LENGTH,
    WRONG_READ_TX_LENGTH,
    WRONG_READ_RX_LENGTH,
    WRONG_REGISTER,
    WRONG_WRITE_VALUE,
    NO_STEP_FOR_MUTATION
  };

  struct ScriptStep {
    TransferKind kind = TransferKind::WRITE_READ;
    uint8_t reg = 0;
    uint16_t writeValue = 0;
    INA3221::Status result = INA3221::Status::Ok();
    WriteEffect writeEffect = WriteEffect::ON_SUCCESS;
    ReadErrorEffect readErrorEffect = ReadErrorEffect::NONE;
    bool mutateAfter = false;
    uint8_t mutationReg = 0;
    uint16_t mutationValue = 0;
  };

  struct CallRecord {
    TransferKind kind = TransferKind::WRITE_READ;
    uint8_t address = 0;
    uint32_t timeoutMs = 0;
    size_t txLen = 0;
    size_t rxLen = 0;
    uint8_t tx[3] = {0, 0, 0};
    uint8_t rx[2] = {0, 0};
    INA3221::Status result = INA3221::Status::Ok();
    bool registerWriteCommitted = false;
    bool destructiveReadOccurred = false;
  };

  explicit ScriptedTransport(uint8_t expectedAddress = 0x40,
                             uint32_t expectedTimeoutMs = 10)
      : _expectedAddress(expectedAddress),
        _expectedTimeoutMs(expectedTimeoutMs) {
    resetDeviceToPowerOnDefaults();
  }

  ScriptedTransport(const ScriptedTransport&) = delete;
  ScriptedTransport& operator=(const ScriptedTransport&) = delete;

  /// Clear expectations, calls, and violations while retaining device state.
  void resetHarness() {
    clearScript();
    clearCalls();
    _violation = Violation::NONE;
  }

  /// Restore the modeled device registers to documented power-on defaults.
  void resetDeviceToPowerOnDefaults() {
    for (size_t i = 0; i < REGISTER_COUNT; ++i) {
      _registers[i] = 0;
    }

    _registers[INA3221::cmd::REG_CONFIG] = INA3221::cmd::CONFIG_DEFAULT;
    _registers[INA3221::cmd::REG_CH1_CRIT_LIMIT] = INA3221::cmd::CRIT_LIMIT_DEFAULT;
    _registers[INA3221::cmd::REG_CH1_WARN_LIMIT] = INA3221::cmd::WARN_LIMIT_DEFAULT;
    _registers[INA3221::cmd::REG_CH2_CRIT_LIMIT] = INA3221::cmd::CRIT_LIMIT_DEFAULT;
    _registers[INA3221::cmd::REG_CH2_WARN_LIMIT] = INA3221::cmd::WARN_LIMIT_DEFAULT;
    _registers[INA3221::cmd::REG_CH3_CRIT_LIMIT] = INA3221::cmd::CRIT_LIMIT_DEFAULT;
    _registers[INA3221::cmd::REG_CH3_WARN_LIMIT] = INA3221::cmd::WARN_LIMIT_DEFAULT;
    _registers[INA3221::cmd::REG_SHUNT_SUM_LIMIT] =
        INA3221::cmd::SHUNT_SUM_LIMIT_DEFAULT;
    _registers[INA3221::cmd::REG_MASK_ENABLE] = INA3221::cmd::MASK_ENABLE_DEFAULT;
    _registers[INA3221::cmd::REG_PV_UPPER_LIMIT] =
        INA3221::cmd::PV_UPPER_LIMIT_DEFAULT;
    _registers[INA3221::cmd::REG_PV_LOWER_LIMIT] =
        INA3221::cmd::PV_LOWER_LIMIT_DEFAULT;
    _registers[INA3221::cmd::REG_MANUFACTURER_ID] =
        INA3221::cmd::MANUFACTURER_ID_VALUE;
    _registers[INA3221::cmd::REG_DIE_ID] = INA3221::cmd::DIE_ID_VALUE;
  }

  void setExpectedTransport(uint8_t address, uint32_t timeoutMs) {
    _expectedAddress = address;
    _expectedTimeoutMs = timeoutMs;
  }

  void setRegister(uint8_t reg, uint16_t value) { _registers[reg] = value; }
  uint16_t registerValue(uint8_t reg) const { return _registers[reg]; }

  /// Queue an exact three-byte register write.
  bool expectWrite(uint8_t reg, uint16_t value,
                   INA3221::Status result = INA3221::Status::Ok(),
                   WriteEffect effect = WriteEffect::ON_SUCCESS) {
    ScriptStep* step = appendStep();
    if (step == nullptr) {
      return false;
    }
    step->kind = TransferKind::WRITE;
    step->reg = reg;
    step->writeValue = value;
    step->result = result;
    step->writeEffect = effect;
    return true;
  }

  /// Queue an exact one-byte register-pointer plus two-byte read.
  bool expectRead(uint8_t reg,
                  INA3221::Status result = INA3221::Status::Ok(),
                  ReadErrorEffect errorEffect = ReadErrorEffect::NONE) {
    ScriptStep* step = appendStep();
    if (step == nullptr) {
      return false;
    }
    step->kind = TransferKind::WRITE_READ;
    step->reg = reg;
    step->result = result;
    step->readErrorEffect = errorEffect;
    return true;
  }

  /// Apply a deterministic register mutation after the most recently queued
  /// transfer, useful for proving mixed-age sequential-read behavior.
  bool mutateAfterLastStep(uint8_t reg, uint16_t value) {
    if (_stepCount == 0) {
      setViolation(Violation::NO_STEP_FOR_MUTATION);
      return false;
    }
    ScriptStep& step = _steps[_stepCount - 1];
    step.mutateAfter = true;
    step.mutationReg = reg;
    step.mutationValue = value;
    return true;
  }

  void clearScript() {
    for (size_t i = 0; i < _stepCount; ++i) {
      _steps[i] = ScriptStep{};
    }
    _stepCount = 0;
    _nextStep = 0;
  }

  void clearCalls() {
    for (size_t i = 0; i < _callCount; ++i) {
      _calls[i] = CallRecord{};
    }
    _callCount = 0;
    _writeCalls = 0;
    _writeReadCalls = 0;
  }

  bool scriptConsumed() const {
    return _violation == Violation::NONE && _nextStep == _stepCount;
  }

  Violation violation() const { return _violation; }
  bool hasViolation() const { return _violation != Violation::NONE; }

  size_t callCount() const { return _callCount; }
  uint32_t writeCallCount() const { return _writeCalls; }
  uint32_t writeReadCallCount() const { return _writeReadCalls; }
  const CallRecord* call(size_t index) const {
    return index < _callCount ? &_calls[index] : nullptr;
  }

  INA3221::TransportConfig makeTransportConfig() {
    INA3221::TransportConfig config;
    config.i2cWrite = &ScriptedTransport::writeCallback;
    config.i2cWriteRead = &ScriptedTransport::writeReadCallback;
    config.i2cUser = this;
    config.nowMs = &ScriptedTransport::nowMsCallback;
    config.cooperativeYield = &ScriptedTransport::yieldCallback;
    config.timeUser = this;
    config.defaultTransferTimeoutMs = _expectedTimeoutMs;
    return config;
  }

  static INA3221::Status writeCallback(uint8_t address, const uint8_t* data,
                                       size_t len, uint32_t timeoutMs,
                                       void* user) {
    if (user == nullptr) {
      return contractViolationStatus(Violation::NULL_BUFFER);
    }
    return static_cast<ScriptedTransport*>(user)->
        handleWrite(address, data, len, timeoutMs);
  }

  static INA3221::Status writeReadCallback(uint8_t address,
                                           const uint8_t* txData, size_t txLen,
                                           uint8_t* rxData, size_t rxLen,
                                           uint32_t timeoutMs, void* user) {
    if (user == nullptr) {
      return contractViolationStatus(Violation::NULL_BUFFER);
    }
    return static_cast<ScriptedTransport*>(user)->
        handleWriteRead(address, txData, txLen, rxData, rxLen, timeoutMs);
  }

  static uint32_t nowMsCallback(void* user) {
    (void)user;
    return 0;
  }

  static void yieldCallback(void* user) {
    (void)user;
  }

private:
  static constexpr size_t REGISTER_COUNT = 256;
  static constexpr uint16_t MASK_ENABLE_READ_CLEAR =
      INA3221::cmd::MASK_CF1 | INA3221::cmd::MASK_CF2 |
      INA3221::cmd::MASK_CF3 | INA3221::cmd::MASK_SF |
      INA3221::cmd::MASK_WF1 | INA3221::cmd::MASK_WF2 |
      INA3221::cmd::MASK_WF3 | INA3221::cmd::MASK_CVRF;

  ScriptStep* appendStep() {
    if (_stepCount >= MAX_SCRIPT_STEPS) {
      setViolation(Violation::SCRIPT_CAPACITY);
      return nullptr;
    }
    _steps[_stepCount] = ScriptStep{};
    return &_steps[_stepCount++];
  }

  CallRecord* appendCall(TransferKind kind, uint8_t address,
                         uint32_t timeoutMs, const uint8_t* txData,
                         size_t txLen, size_t rxLen) {
    if (_callCount >= MAX_CALLS) {
      setViolation(Violation::CALL_CAPACITY);
      return nullptr;
    }
    CallRecord& record = _calls[_callCount++];
    record = CallRecord{};
    record.kind = kind;
    record.address = address;
    record.timeoutMs = timeoutMs;
    record.txLen = txLen;
    record.rxLen = rxLen;
    if (txData != nullptr) {
      const size_t copyLen = txLen < sizeof(record.tx) ? txLen : sizeof(record.tx);
      for (size_t i = 0; i < copyLen; ++i) {
        record.tx[i] = txData[i];
      }
    }
    return &record;
  }

  ScriptStep* consumeStep(TransferKind actualKind) {
    if (_nextStep >= _stepCount) {
      setViolation(Violation::UNEXPECTED_CALL);
      return nullptr;
    }
    ScriptStep* step = &_steps[_nextStep++];
    if (step->kind != actualKind) {
      setViolation(Violation::WRONG_KIND);
      return nullptr;
    }
    return step;
  }

  bool validateCommon(uint8_t address, uint32_t timeoutMs) {
    if (address != _expectedAddress) {
      setViolation(Violation::WRONG_ADDRESS);
      return false;
    }
    if (timeoutMs != _expectedTimeoutMs) {
      setViolation(Violation::WRONG_TIMEOUT);
      return false;
    }
    return true;
  }

  INA3221::Status handleWrite(uint8_t address, const uint8_t* data,
                              size_t len, uint32_t timeoutMs) {
    if (_writeCalls != UINT32_MAX) {
      ++_writeCalls;
    }
    CallRecord* callRecord = appendCall(TransferKind::WRITE, address, timeoutMs,
                                        data, len, 0);
    ScriptStep* step = consumeStep(TransferKind::WRITE);

    Violation failure =
        callRecord == nullptr ? Violation::CALL_CAPACITY : Violation::NONE;
    if (failure != Violation::NONE) {
      // Preserve the bounded-call-log failure.
    } else if (!validateCommon(address, timeoutMs)) {
      failure = _violation;
    } else if (data == nullptr) {
      failure = Violation::NULL_BUFFER;
    } else if (len != 3) {
      failure = Violation::WRONG_WRITE_LENGTH;
    } else if (step == nullptr) {
      failure = _violation;
    } else if (data[0] != step->reg) {
      failure = Violation::WRONG_REGISTER;
    } else {
      const uint16_t value =
          static_cast<uint16_t>((static_cast<uint16_t>(data[1]) << 8) | data[2]);
      if (value != step->writeValue) {
        failure = Violation::WRONG_WRITE_VALUE;
      }
    }

    if (failure != Violation::NONE) {
      setViolation(failure);
      const INA3221::Status result = contractViolationStatus(failure);
      if (callRecord != nullptr) {
        callRecord->result = result;
      }
      return result;
    }

    const bool commit = step->result.ok() ||
                        step->writeEffect == WriteEffect::COMMIT_BEFORE_STATUS;
    if (commit) {
      applyRegisterWrite(step->reg, step->writeValue);
    }
    if (step->mutateAfter) {
      _registers[step->mutationReg] = step->mutationValue;
    }
    if (callRecord != nullptr) {
      callRecord->registerWriteCommitted = commit;
      callRecord->result = step->result;
    }
    return step->result;
  }

  INA3221::Status handleWriteRead(uint8_t address, const uint8_t* txData,
                                  size_t txLen, uint8_t* rxData, size_t rxLen,
                                  uint32_t timeoutMs) {
    if (_writeReadCalls != UINT32_MAX) {
      ++_writeReadCalls;
    }
    CallRecord* callRecord = appendCall(TransferKind::WRITE_READ, address,
                                        timeoutMs, txData, txLen, rxLen);
    ScriptStep* step = consumeStep(TransferKind::WRITE_READ);

    Violation failure =
        callRecord == nullptr ? Violation::CALL_CAPACITY : Violation::NONE;
    if (failure != Violation::NONE) {
      // Preserve the bounded-call-log failure.
    } else if (!validateCommon(address, timeoutMs)) {
      failure = _violation;
    } else if (txData == nullptr || rxData == nullptr) {
      failure = Violation::NULL_BUFFER;
    } else if (txLen != 1) {
      failure = Violation::WRONG_READ_TX_LENGTH;
    } else if (rxLen != 2) {
      failure = Violation::WRONG_READ_RX_LENGTH;
    } else if (step == nullptr) {
      failure = _violation;
    } else if (txData[0] != step->reg) {
      failure = Violation::WRONG_REGISTER;
    }

    if (failure != Violation::NONE) {
      setViolation(failure);
      const INA3221::Status result = contractViolationStatus(failure);
      if (callRecord != nullptr) {
        callRecord->result = result;
      }
      return result;
    }

    const uint16_t value = _registers[step->reg];
    const bool fullData = step->result.ok() ||
                          step->readErrorEffect == ReadErrorEffect::FULL_DATA;
    const bool firstByte =
        step->readErrorEffect == ReadErrorEffect::FIRST_BYTE_ONLY;
    if (fullData || firstByte) {
      rxData[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
    }
    if (fullData) {
      rxData[1] = static_cast<uint8_t>(value & 0xFF);
    }

    const bool destructiveRead = fullData || firstByte;
    if (destructiveRead && step->reg == INA3221::cmd::REG_MASK_ENABLE) {
      _registers[step->reg] = static_cast<uint16_t>(
          _registers[step->reg] & static_cast<uint16_t>(~MASK_ENABLE_READ_CLEAR));
    }
    if (step->mutateAfter) {
      _registers[step->mutationReg] = step->mutationValue;
    }
    if (callRecord != nullptr) {
      if (fullData || firstByte) {
        callRecord->rx[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
      }
      if (fullData) {
        callRecord->rx[1] = static_cast<uint8_t>(value & 0xFF);
      }
      callRecord->destructiveReadOccurred =
          destructiveRead && step->reg == INA3221::cmd::REG_MASK_ENABLE;
      callRecord->result = step->result;
    }
    return step->result;
  }

  void applyRegisterWrite(uint8_t reg, uint16_t value) {
    if (reg == INA3221::cmd::REG_CONFIG &&
        (value & INA3221::cmd::MASK_RST) != 0U) {
      resetDeviceToPowerOnDefaults();
      return;
    }
    _registers[reg] = value;
  }

  void setViolation(Violation violation) {
    if (_violation == Violation::NONE) {
      _violation = violation;
    }
  }

  static INA3221::Status contractViolationStatus(Violation violation) {
    return INA3221::Status::Error(
        INA3221::Err::INVALID_PARAM,
        "Scripted transport contract violation",
        static_cast<int32_t>(violation));
  }

  uint8_t _expectedAddress = 0x40;
  uint32_t _expectedTimeoutMs = 10;
  uint16_t _registers[REGISTER_COUNT] = {};

  ScriptStep _steps[MAX_SCRIPT_STEPS] = {};
  size_t _stepCount = 0;
  size_t _nextStep = 0;

  CallRecord _calls[MAX_CALLS] = {};
  size_t _callCount = 0;
  uint32_t _writeCalls = 0;
  uint32_t _writeReadCalls = 0;
  Violation _violation = Violation::NONE;

};

}  // namespace INA3221Test
