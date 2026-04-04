/// @file CommandTable.h
/// @brief Register addresses, bit definitions, and constants for INA3221
#pragma once

#include <cstdint>

namespace INA3221 {
namespace cmd {

// ============================================================================
// Register Addresses
// ============================================================================

static constexpr uint8_t REG_CONFIG          = 0x00;  ///< Configuration register (R/W)
static constexpr uint8_t REG_CH1_SHUNT       = 0x01;  ///< Channel-1 shunt voltage (R)
static constexpr uint8_t REG_CH1_BUS         = 0x02;  ///< Channel-1 bus voltage (R)
static constexpr uint8_t REG_CH2_SHUNT       = 0x03;  ///< Channel-2 shunt voltage (R)
static constexpr uint8_t REG_CH2_BUS         = 0x04;  ///< Channel-2 bus voltage (R)
static constexpr uint8_t REG_CH3_SHUNT       = 0x05;  ///< Channel-3 shunt voltage (R)
static constexpr uint8_t REG_CH3_BUS         = 0x06;  ///< Channel-3 bus voltage (R)
static constexpr uint8_t REG_CH1_CRIT_LIMIT  = 0x07;  ///< Channel-1 critical alert limit (R/W)
static constexpr uint8_t REG_CH1_WARN_LIMIT  = 0x08;  ///< Channel-1 warning alert limit (R/W)
static constexpr uint8_t REG_CH2_CRIT_LIMIT  = 0x09;  ///< Channel-2 critical alert limit (R/W)
static constexpr uint8_t REG_CH2_WARN_LIMIT  = 0x0A;  ///< Channel-2 warning alert limit (R/W)
static constexpr uint8_t REG_CH3_CRIT_LIMIT  = 0x0B;  ///< Channel-3 critical alert limit (R/W)
static constexpr uint8_t REG_CH3_WARN_LIMIT  = 0x0C;  ///< Channel-3 warning alert limit (R/W)
static constexpr uint8_t REG_SHUNT_SUM       = 0x0D;  ///< Shunt-voltage sum (R)
static constexpr uint8_t REG_SHUNT_SUM_LIMIT = 0x0E;  ///< Shunt-voltage sum limit (R/W)
static constexpr uint8_t REG_MASK_ENABLE     = 0x0F;  ///< Mask/Enable register (R/W)
static constexpr uint8_t REG_PV_UPPER_LIMIT  = 0x10;  ///< Power-valid upper limit (R/W)
static constexpr uint8_t REG_PV_LOWER_LIMIT  = 0x11;  ///< Power-valid lower limit (R/W)
static constexpr uint8_t REG_MANUFACTURER_ID = 0xFE;  ///< Manufacturer ID (R)
static constexpr uint8_t REG_DIE_ID          = 0xFF;  ///< Die ID (R)

// ============================================================================
// Default Register Values (Power-On Reset)
// ============================================================================

static constexpr uint16_t CONFIG_DEFAULT          = 0x7127;
static constexpr uint16_t SHUNT_VOLTAGE_DEFAULT   = 0x0000;
static constexpr uint16_t BUS_VOLTAGE_DEFAULT     = 0x0000;
static constexpr uint16_t CRIT_LIMIT_DEFAULT      = 0x7FF8;
static constexpr uint16_t WARN_LIMIT_DEFAULT      = 0x7FF8;
static constexpr uint16_t SHUNT_SUM_DEFAULT       = 0x0000;
static constexpr uint16_t SHUNT_SUM_LIMIT_DEFAULT = 0x7FFE;
static constexpr uint16_t MASK_ENABLE_DEFAULT     = 0x0002;
static constexpr uint16_t PV_UPPER_LIMIT_DEFAULT  = 0x2710;  ///< 10.000 V
static constexpr uint16_t PV_LOWER_LIMIT_DEFAULT  = 0x2328;  ///< 9.000 V
static constexpr uint16_t MANUFACTURER_ID_VALUE   = 0x5449;  ///< "TI" in ASCII
static constexpr uint16_t DIE_ID_VALUE            = 0x3220;  ///< INA3221 Die ID

// ============================================================================
// Configuration Register (0x00) Bit Masks
// ============================================================================

static constexpr uint16_t MASK_RST    = 0x8000;  ///< System reset (bit 15)
static constexpr uint16_t MASK_CH1EN  = 0x4000;  ///< Channel 1 enable (bit 14)
static constexpr uint16_t MASK_CH2EN  = 0x2000;  ///< Channel 2 enable (bit 13)
static constexpr uint16_t MASK_CH3EN  = 0x1000;  ///< Channel 3 enable (bit 12)
static constexpr uint16_t MASK_AVG    = 0x0E00;  ///< Averaging mode (bits 11:9)
static constexpr uint16_t MASK_VBUSCT = 0x01C0;  ///< Bus voltage conversion time (bits 8:6)
static constexpr uint16_t MASK_VSHCT  = 0x0038;  ///< Shunt voltage conversion time (bits 5:3)
static constexpr uint16_t MASK_MODE   = 0x0007;  ///< Operating mode (bits 2:0)

// ============================================================================
// Configuration Register Bit Positions
// ============================================================================

static constexpr uint8_t BIT_RST    = 15;
static constexpr uint8_t BIT_CH1EN  = 14;
static constexpr uint8_t BIT_CH2EN  = 13;
static constexpr uint8_t BIT_CH3EN  = 12;
static constexpr uint8_t BIT_AVG    = 9;   ///< AVG[2:0] bits 11:9
static constexpr uint8_t BIT_VBUSCT = 6;   ///< VBUSCT[2:0] bits 8:6
static constexpr uint8_t BIT_VSHCT  = 3;   ///< VSHCT[2:0] bits 5:3
static constexpr uint8_t BIT_MODE   = 0;   ///< MODE[2:0] bits 2:0

// ============================================================================
// Mask/Enable Register (0x0F) Bit Masks
// ============================================================================

static constexpr uint16_t MASK_SCC1 = 0x4000;  ///< Summation channel control Ch1 (bit 14)
static constexpr uint16_t MASK_SCC2 = 0x2000;  ///< Summation channel control Ch2 (bit 13)
static constexpr uint16_t MASK_SCC3 = 0x1000;  ///< Summation channel control Ch3 (bit 12)
static constexpr uint16_t MASK_WEN  = 0x0800;  ///< Warning alert latch enable (bit 11)
static constexpr uint16_t MASK_CEN  = 0x0400;  ///< Critical alert latch enable (bit 10)
static constexpr uint16_t MASK_CF1  = 0x0200;  ///< Critical flag Ch1 (bit 9)
static constexpr uint16_t MASK_CF2  = 0x0100;  ///< Critical flag Ch2 (bit 8)
static constexpr uint16_t MASK_CF3  = 0x0080;  ///< Critical flag Ch3 (bit 7)
static constexpr uint16_t MASK_SF   = 0x0040;  ///< Summation flag (bit 6)
static constexpr uint16_t MASK_WF1  = 0x0020;  ///< Warning flag Ch1 (bit 5)
static constexpr uint16_t MASK_WF2  = 0x0010;  ///< Warning flag Ch2 (bit 4)
static constexpr uint16_t MASK_WF3  = 0x0008;  ///< Warning flag Ch3 (bit 3)
static constexpr uint16_t MASK_PVF  = 0x0004;  ///< Power valid flag (bit 2)
static constexpr uint16_t MASK_TCF  = 0x0002;  ///< Timing control flag (bit 1)
static constexpr uint16_t MASK_CVRF = 0x0001;  ///< Conversion ready flag (bit 0)

// ============================================================================
// Mask/Enable Register Bit Positions
// ============================================================================

static constexpr uint8_t BIT_SCC1 = 14;
static constexpr uint8_t BIT_SCC2 = 13;
static constexpr uint8_t BIT_SCC3 = 12;
static constexpr uint8_t BIT_WEN  = 11;
static constexpr uint8_t BIT_CEN  = 10;
static constexpr uint8_t BIT_CF1  = 9;
static constexpr uint8_t BIT_CF2  = 8;
static constexpr uint8_t BIT_CF3  = 7;
static constexpr uint8_t BIT_SF   = 6;
static constexpr uint8_t BIT_WF1  = 5;
static constexpr uint8_t BIT_WF2  = 4;
static constexpr uint8_t BIT_WF3  = 3;
static constexpr uint8_t BIT_PVF  = 2;
static constexpr uint8_t BIT_TCF  = 1;
static constexpr uint8_t BIT_CVRF = 0;

// ============================================================================
// Data Register Format
// ============================================================================

/// Data in bits [15:3], bits [2:0] reserved (always 0). Two's complement.
static constexpr uint8_t DATA_SHIFT = 3;

/// Shunt-voltage LSB = 40 µV
static constexpr float SHUNT_LSB_UV = 40.0f;
static constexpr float SHUNT_LSB_MV = 0.04f;

/// Bus-voltage LSB = 8 mV
static constexpr float BUS_LSB_MV = 8.0f;
static constexpr float BUS_LSB_V  = 0.008f;

/// Shunt-voltage sum LSB = 40 µV (bit 0 reserved, data in bits [15:1])
static constexpr uint8_t SUM_DATA_SHIFT = 1;

// ============================================================================
// Field Values
// ============================================================================

// AVG field values
static constexpr uint16_t AVG_1    = 0x0000;  ///< 1 sample (default)
static constexpr uint16_t AVG_4    = 0x0200;  ///< 4 samples
static constexpr uint16_t AVG_16   = 0x0400;  ///< 16 samples
static constexpr uint16_t AVG_64   = 0x0600;  ///< 64 samples
static constexpr uint16_t AVG_128  = 0x0800;  ///< 128 samples
static constexpr uint16_t AVG_256  = 0x0A00;  ///< 256 samples
static constexpr uint16_t AVG_512  = 0x0C00;  ///< 512 samples
static constexpr uint16_t AVG_1024 = 0x0E00;  ///< 1024 samples

// VBUSCT field values
static constexpr uint16_t VBUSCT_140US   = 0x0000;  ///< 140 µs
static constexpr uint16_t VBUSCT_204US   = 0x0040;  ///< 204 µs
static constexpr uint16_t VBUSCT_332US   = 0x0080;  ///< 332 µs
static constexpr uint16_t VBUSCT_588US   = 0x00C0;  ///< 588 µs
static constexpr uint16_t VBUSCT_1100US  = 0x0100;  ///< 1.1 ms (default)
static constexpr uint16_t VBUSCT_2116US  = 0x0140;  ///< 2.116 ms
static constexpr uint16_t VBUSCT_4156US  = 0x0180;  ///< 4.156 ms
static constexpr uint16_t VBUSCT_8244US  = 0x01C0;  ///< 8.244 ms

// VSHCT field values
static constexpr uint16_t VSHCT_140US   = 0x0000;  ///< 140 µs
static constexpr uint16_t VSHCT_204US   = 0x0008;  ///< 204 µs
static constexpr uint16_t VSHCT_332US   = 0x0010;  ///< 332 µs
static constexpr uint16_t VSHCT_588US   = 0x0018;  ///< 588 µs
static constexpr uint16_t VSHCT_1100US  = 0x0020;  ///< 1.1 ms (default)
static constexpr uint16_t VSHCT_2116US  = 0x0028;  ///< 2.116 ms
static constexpr uint16_t VSHCT_4156US  = 0x0030;  ///< 4.156 ms
static constexpr uint16_t VSHCT_8244US  = 0x0038;  ///< 8.244 ms

// MODE field values
static constexpr uint16_t MODE_POWER_DOWN      = 0x0000;  ///< Power-down
static constexpr uint16_t MODE_SHUNT_TRIG      = 0x0001;  ///< Shunt voltage, single-shot
static constexpr uint16_t MODE_BUS_TRIG        = 0x0002;  ///< Bus voltage, single-shot
static constexpr uint16_t MODE_SHUNT_BUS_TRIG  = 0x0003;  ///< Shunt + bus, single-shot
static constexpr uint16_t MODE_POWER_DOWN_ALT  = 0x0004;  ///< Power-down (alternate)
static constexpr uint16_t MODE_SHUNT_CONT      = 0x0005;  ///< Shunt voltage, continuous
static constexpr uint16_t MODE_BUS_CONT        = 0x0006;  ///< Bus voltage, continuous
static constexpr uint16_t MODE_SHUNT_BUS_CONT  = 0x0007;  ///< Shunt + bus, continuous (default)

// ============================================================================
// Channel Index Helpers
// ============================================================================

/// Register offset per channel for shunt and bus voltage
static constexpr uint8_t SHUNT_REG_BASE = REG_CH1_SHUNT;  ///< 0x01
static constexpr uint8_t BUS_REG_BASE   = REG_CH1_BUS;    ///< 0x02
static constexpr uint8_t CRIT_REG_BASE  = REG_CH1_CRIT_LIMIT;  ///< 0x07
static constexpr uint8_t WARN_REG_BASE  = REG_CH1_WARN_LIMIT;  ///< 0x08

/// Stride between consecutive channel register pairs
static constexpr uint8_t CHANNEL_STRIDE = 2;

} // namespace cmd
} // namespace INA3221
