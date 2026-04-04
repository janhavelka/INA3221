# INA3221 — Triple-Channel, High-Side Current and Power Monitor — Extraction

---

## 1. Source Documents

| # | Filename | Description | Pages | Size |
|---|----------|-------------|-------|------|
| 1 | `datasheet_INA3221.pdf` | INA3221 26V, Triple Channel, 13-Bit, I2C Output Current and Voltage Monitor With Alerts — SBOS576C, May 2012, Rev. Sep 2025 | 50 | ~113 KB |
| 2 | `current_sense_amplifiers.pdf` | Current Sense Amplifiers Guide 2021 — TI selection guide | 8 | small |
| 3 | `current_sensing_application.pdf` | Current Sensing Applications in Communication Infrastructure Equipment — SBOA366C, Dec 2020, Rev. Aug 2023 | 4 | small |
| 4 | `power_energy_monitoring.pdf` | Power and Energy Monitoring With Digital Current Sensors — SBOA194, Apr 2017 | 3 | small |
| 5 | `esd_risks_and_prevention.pdf` | Risks and Prevention of ESD, EOS, and Latch Up Events for Current Sense Amplifiers — SBOA615, Nov 2024 | 10 | small |
| 6 | `ebook_current_sensing.pdf` | Current Sensing eBook (multi-chapter compendium) — includes SLYA024A | ~210 KB | large |

Primary source for all register/bitfield/timing data: document #1 (datasheet).  
Documents #2–#6 provide application context only; no INA3221-specific register detail beyond the datasheet.

---

## 2. Device Identity and Variants

| Field | Value | Source |
|-------|-------|--------|
| Device name | INA3221 | DS p.1 |
| Full title | 26V, Triple Channel, 13-Bit, I2C Output Current and Voltage Monitor With Alerts | DS p.1 |
| Document number | SBOS576C | DS p.1 |
| Package | RGV (VQFN-16), 4.00 mm × 4.00 mm | DS p.1 |
| Manufacturer ID register (0xFE) | `0x5449` ("TI" in ASCII) | DS p.36 |
| Die ID register (0xFF) | `0x3220` | DS p.36 |
| Part marking | "INA 3221" (two-line) | DS p.42 |
| Orderable PNs | INA3221AIRGVR, INA3221AIRGVR.B, INA3221AIRGVRG4, INA3221AIRGVRG4.B | DS p.42 |
| Obsolete PN | INA3221AIRGVT | DS p.42 |
| Operating temperature | −40 °C to +125 °C | DS p.4 |
| RoHS | Yes | DS p.42 |
| MSL rating | Level-1-260C-UNLIM | DS p.42 |

**Related/comparable devices** (DS p.3):

| Device | Description |
|--------|-------------|
| INA226 | 36V, Bidirectional, Ultrahigh Accuracy, I2C, Current and Power Monitor With Alert |
| INA219 | 26V, Bidirectional, Zero-Drift, High-Side, I2C, Current and Power Monitor |
| INA209 | 26V, Bidirectional, I2C, Current and Power Monitor and High-Speed Comparator |
| INA210-214 | 26V, Bidirectional, Zero-Drift, Voltage Out, Current Shunt Monitor |

From app note #4 (SBOA194 p.2): INA3221 described as "monitors 3 channels" with trade-off of "less accuracy and resolution, monitors bus and shunt voltages" vs. INA233.

---

## 3. High-Level Functional Summary

The INA3221 is a **three-channel, high-side current and bus voltage monitor** with I2C/SMBus interface. (DS p.1, p.10)

Key capabilities:
- Measures **shunt voltage** (across external shunt resistor, between IN+ and IN−) and **bus voltage** (IN− to GND) on each of 3 independent channels. (DS p.11)
- Bus voltage sensing range: **0 V to 26 V**. (DS p.1)
- Shunt voltage range: **−163.84 mV to +163.8 mV**. (DS p.5)
- **13-bit ADC** with configurable conversion times (140 µs to 8.244 ms) and averaging (1 to 1024 samples). (DS p.5, p.26)
- **Four alert outputs** (all open-drain): Critical, Warning, PV (Power Valid), TC (Timing Control). (DS p.3, p.12)
- Critical alert: per-channel, compares **single conversion** against limit. (DS p.12)
- Warning alert: per-channel, compares **averaged value** against limit. (DS p.12)
- Summation channel: sums selected channel shunt voltages, compares against sum limit → drives Critical pin. (DS p.12)
- Power-valid window: monitors all bus voltages against upper/lower thresholds → drives PV pin. (DS p.13)
- Timing control: verifies power-supply sequencing at startup → drives TC pin. (DS p.14)
- Powered from **2.7 V to 5.5 V** supply, draws **350 µA typ** (active), **0.5 µA typ** (power-down). (DS p.1, p.6)
- Four programmable I2C addresses via A0 pin. (DS p.20)

From app note #3 (SBOA366C p.2): INA3221 described as "a multichannel digital monitor…frees up controller ADC channels while taking advantage of an existing I2C bus. This device offers a number of warning and alert signals for fast action in case of a fault, as well as current, voltage, or power information of three independent channels."

---

## 4. Interface Summary

| Parameter | Value | Source |
|-----------|-------|--------|
| Interface type | I2C and SMBus compatible | DS p.1, p.20 |
| I2C modes supported | Fast mode (up to 400 kHz), High-speed mode (up to 2.44 MHz) | DS p.20 |
| Data format | 16-bit registers, MSB first | DS p.20 |
| Address pins | 1 pin (A0) | DS p.20 |
| Number of addresses | 4 | DS p.20 |
| SDA | Open-drain I/O | DS p.3 |
| SCL | Open-drain input | DS p.3 |
| Spike suppression | Integrated on SDA and SCL | DS p.20 |
| SMBus timeout | 28 ms min, 35 ms max (resets interface when SCL low for >28 ms) | DS p.5–6 |
| SMBus Alert Response | Supported; target address 0001 100 with R/W=1 | DS p.23 |
| Byte order | MSByte first, then LSByte | DS p.21 |

**I2C Address Table** (DS p.20, Table 7-1):

| A0 pin connection | 7-bit Target Address (binary) | 7-bit Address (hex) |
|--------------------|-------------------------------|---------------------|
| GND | 1000000 | 0x40 |
| VS | 1000001 | 0x41 |
| SDA | 1000010 | 0x42 |
| SCL | 1000011 | 0x43 |

The state of A0 is sampled on every bus communication. (DS p.20)

**Write transaction** (DS p.21, Fig. 7-10):
```
[START] [7-bit addr + W] [ACK] [register pointer byte] [ACK] [data MSByte] [ACK] [data LSByte] [ACK] [STOP]
```

**Read transaction** (DS p.21, Fig. 7-11):
```
; First: set register pointer (write)
[START] [7-bit addr + W] [ACK] [register pointer byte] [ACK]
; Then: read data (repeated start or new start)
[START] [7-bit addr + R] [ACK] [data MSByte from device] [ACK by controller] [data LSByte from device] [NACK by controller] [STOP]
```

Register pointer is retained until changed by a new write. Repeated reads from the same register do not require re-sending the pointer. (DS p.21)

**High-Speed I2C Mode** (DS p.22):
1. Controller sends START + Hs master code `00001XXX` at ≤ 400 kHz (F/S mode).
2. INA3221 does NOT acknowledge the Hs code but switches internal filters for 2.44 MHz.
3. Controller sends repeated START, then proceeds at up to 2.44 MHz.
4. A STOP condition ends Hs mode and reverts to F/S filters.

**Bus Timing** (DS p.23, Table 7-2):

| Parameter | Fast Mode Min | Fast Mode Max | High-Speed Min | High-Speed Max | Unit |
|-----------|---------------|---------------|----------------|----------------|------|
| f(SCL) | 0.001 | 0.4 | 0.001 | 2.44 | MHz |
| t(BUF) bus free time | 1300 | — | 160 | — | ns |
| t(HDSTA) hold after START | 600 | — | 160 | — | ns |
| t(SUSTA) repeated START setup | 600 | — | 160 | — | ns |
| t(SUSTO) STOP setup | 600 | — | 160 | — | ns |
| t(HDDAT) data hold | 0 | — | 0 | — | ns |
| t(VDDAT) data valid | — | 1200 | — | 260 | ns |
| t(SUDAT) data setup | 100 | — | 10 | — | ns |
| t(LOW) SCL low | 1300 | — | 270 | — | ns |
| t(HIGH) SCL high | 600 | — | 60 | — | ns |
| tfDA data fall | — | 500 | — | 150 | ns |
| tfCL clock fall | — | 300 | — | 40 | ns |
| tr clock rise | — | 300 | — | 40 | ns |
| tr (SCL ≤ 100 kHz) | — | 1000 | — | — | ns |

Note: "Values based on a statistical analysis of a one-time sample of devices. Minimum and maximum values are not production tested." (DS p.23)

---

## 5. Electrical and Timing Constraints Relevant to Software

### Absolute Maximum Ratings (DS p.4)

| Parameter | Min | Max | Unit |
|-----------|-----|-----|------|
| Supply VS | — | 6 | V |
| Analog inputs IN+, IN− differential | −26 | 26 | V |
| Analog inputs IN+, IN− common-mode | −0.3 | 26 | V |
| VPU | — | 26 | V |
| Digital outputs (Critical, Warning, PV) | — | 6 | V |
| TC output | — | 26 | V |
| SDA | GND − 0.3 | 6 | V |
| SCL | GND − 0.3 | VS + 0.3 | V |
| Input current, any pin | — | 5 | mA |
| Open-drain digital output current | — | 10 | mA |
| Operating temp TA | −40 | 125 | °C |
| Junction temp TJ | — | 150 | °C |
| Storage temp | −65 | 150 | °C |

**CAUTION**: VIN+ and VIN− can have differential of −26 V to +26 V, but the absolute voltage at these pins must not exceed −0.3 V to +26 V. (DS p.4)

### ESD Ratings (DS p.4)

| Model | Value |
|-------|-------|
| HBM (ANSI/ESDA/JEDEC JS-001) | ±2500 V |
| CDM (JESD22-C101) | ±1000 V |
| Machine model | ±200 V |

### Recommended Operating Conditions (DS p.4)

| Parameter | Min | Nom | Max | Unit |
|-----------|-----|-----|-----|------|
| Supply voltage | 2.7 | — | 5.5 | V |
| Operating temp | −40 | — | 125 | °C |

### Key Electrical Characteristics (DS p.5–6, at TA=25°C, VS=3.3V, VIN+=12V)

| Parameter | Min | Typ | Max | Unit |
|-----------|-----|-----|-----|------|
| Shunt voltage input range | −163.84 | — | 163.8 | mV |
| Bus voltage input range | 0 | — | 26 | V |
| CMR (common-mode rejection) | 110 | 120 | — | dB |
| Shunt offset voltage (VOS) | — | ±40 | ±80 | µV |
| Shunt offset drift | — | 0.1 | 0.5 | µV/°C |
| Shunt PSRR | — | 15 | — | µV/V |
| Bus offset voltage | — | ±8 | ±16 | mV |
| Bus offset drift | — | — | 80 | µV/°C |
| Bus PSRR | — | 0.5 | — | mV/V |
| IIN+ bias current | — | 10 | — | µA |
| IIN− bias current | — | 10 / 670 | — | µA / kΩ |
| Input leakage (power-down) | — | 0.1 | 0.5 | µA |
| ADC resolution | — | 13 | — | bits |
| Shunt voltage LSB | — | 40 | — | µV |
| Bus voltage LSB | — | 8 | — | mV |
| Shunt gain error | — | 0.1 | 0.25 | % |
| Shunt gain error drift | — | 10 | 50 | ppm/°C |
| Bus gain error | — | 0.1 | 0.25 | % |
| Bus gain error drift | — | 10 | 50 | ppm/°C |
| DNL | — | — | ±0.1 | LSB |
| Quiescent current (active) | — | 350 | 450 | µA |
| Quiescent current (power-down) | — | 0.5 | 2 | µA |
| Power-on reset threshold | — | — | 2 | V |
| Digital VIH | 0.7 × VS | — | 6 | V |
| Digital VIL | −0.5 | — | 0.3 × VS | V |
| VOL (SDA, Critical, Warning, PV) | 0 | — | 0.4 | V (at IOL = 3 mA) |
| VOL (TC) | 0 | — | 0.4 | V (at IOL = 1.2 mA) |
| Hysteresis voltage | — | 500 | — | mV |
| Input capacitance | — | 3 | — | pF |
| Digital leakage | — | 0.1 | 1 | µA |

### ADC Conversion Times (DS p.5)

| CT bits [2:0] | Typ | Max | Unit |
|---------------|-----|-----|------|
| 000 | 140 | 154 | µs |
| 001 | 204 | 224 | µs |
| 010 | 332 | 365 | µs |
| 011 | 588 | 646 | µs |
| 100 | 1.1 | 1.21 | ms |
| 101 | 2.116 | 2.328 | ms |
| 110 | 4.156 | 4.572 | ms |
| 111 | 8.244 | 9.068 | ms |

### Internal ADC Architecture (DS p.19)
- Delta-sigma (ΔΣ) front-end with **500 kHz (±30%)** typical sampling rate.
- Good inherent noise rejection; transients at sampling-rate harmonics (≥ 1 MHz) can cause issues.
- Filter recommendation: ≤ 10 Ω series resistor + 0.1 µF to 1.0 µF ceramic cap at inputs. (DS p.19)

---

## 6. Power, Reset, Enable, and Startup Behavior

### Power Supply (DS p.1, p.4, p.38)
- VS range: **2.7 V to 5.5 V**. (DS p.4)
- Bypass cap: **0.1 µF ceramic**, as close as possible to VS and GND pins. (DS p.37–39)
- POR (power-on reset) threshold: **2 V**. (DS p.6)
- At POR, all registers initialize to default values shown in Table 7-3. (DS p.15, p.24)

### Power-Up Sequence (DS p.11)
- No special power-supply sequencing required between common-mode inputs and VS — they are independent. Bus voltages can be present with supply off and vice versa. (DS p.11)
- At power-up, device enters **continuous shunt and bus voltage mode** (MODE = 111, default). (DS p.27)
- All three channels enabled by default (CH1en = CH2en = CH3en = 1). (DS p.26)

### Software Reset (DS p.15)
- Set **bit 15 (RST)** of Configuration register (0x00) to 1.
- Reinitializes all registers to default power-up values.
- RST bit is **self-clearing**.
- **Exception**: PV pin output is held until power-valid detection sequence completes after reset. Power-Valid Upper/Lower Limit registers DO reset to defaults (10 V / 9 V). (DS p.15)

### Power-Down Mode (DS p.11)
- MODE bits = 000 or 100 in Configuration register → power-down.
- Quiescent current drops to **0.5 µA typ, 2 µA max**. (DS p.6)
- Turns off current into INA3221 inputs. (DS p.11)
- Input leakage in power-down: 0.1 µA typ, 0.5 µA max. (DS p.5)
- Registers can be read/written in power-down mode. (DS p.11)
- **Full recovery from power-down: 40 µs**. (DS p.11)
- Device remains in power-down until an active MODE is written to Configuration register. (DS p.11)

### Channel Enable/Disable (DS p.17, p.26)
- Configuration register bits 14–12: CH1en, CH2en, CH3en.
- 0 = disabled, 1 = enabled (default = all enabled). (DS p.26)
- Disabled channels are bypassed in the measurement sequence regardless of mode. (DS p.11)
- Disabling unused channels improves response time on remaining channels. (DS p.17)

---

## 7. Pin Behavior Relevant to Firmware

### Pin Table (DS p.3, Table 5-1)

| Pin | Number | Type | Description |
|-----|--------|------|-------------|
| A0 | 5 | Digital input | Address pin. Connect to GND, VS, SDA, or SCL for 4 addresses. |
| Critical | 9 | Digital output | Conversion-triggered critical alert; **open-drain**. |
| GND | 3 | Analog | Ground |
| IN−1 | 11 | Analog input | Load side of Ch1 shunt. Bus voltage measured from here to GND. |
| IN+1 | 12 | Analog input | Supply side of Ch1 shunt. |
| IN−2 | 14 | Analog input | Load side of Ch2 shunt. |
| IN+2 | 15 | Analog input | Supply side of Ch2 shunt. |
| IN−3 | 1 | Analog input | Load side of Ch3 shunt. |
| IN+3 | 2 | Analog input | Supply side of Ch3 shunt. |
| PV | 10 | Digital output | Power valid alert; **open-drain**. Active-HIGH when valid. |
| SCL | 6 | Digital input | I2C clock; open-drain input. |
| SDA | 7 | Digital I/O | I2C data; open-drain I/O. |
| TC | 13 | Digital output | Timing control alert; **open-drain**. |
| VPU | 16 | Analog input | Pull-up supply for PV output circuitry. |
| VS | 4 | Analog | Power supply, 2.7 V to 5.5 V. |
| Thermal pad | — | — | Must connect to GND or leave floating. |

### Alert Pin Behavior Summary

| Pin | Drive | Polarity | Pull-up needed | VOL spec |
|-----|-------|----------|----------------|----------|
| Critical | Open-drain | Active LOW | Yes (to VPU or VS) | ≤ 0.4 V at 3 mA (DS p.5) |
| Warning | Open-drain | Active LOW | Yes (to VPU or VS) | ≤ 0.4 V at 3 mA (DS p.5) |
| PV | Open-drain | **Active HIGH = power valid** (pulled up via VPU/RPU) | Yes (to VPU) | ≤ 0.4 V at 3 mA (DS p.5) |
| TC | Open-drain | Active LOW = timing fail | Yes | ≤ 0.4 V at 1.2 mA (DS p.5) |

- PV pin powers up LOW and goes HIGH when all enabled channels exceed the Power-Valid Upper Limit. (DS p.13)
- PV output high level is set by VPU pull-up. Can be voltage-divided with RDIV to interface with lower-voltage logic. (DS p.14)
- TC pin: monitored only at power-up or after software reset. Writing Configuration register before TC sequence completes disables TC until next power cycle or software reset. (DS p.15)

### A0 Pin Behavior
- Sampled on every bus communication (not latched at power-up). (DS p.20)
- Must be set before any I2C activity. (DS p.20)
- Connect to GND, VS, SDA, or SCL only. (DS p.20)

---

## 8. Register Map Overview

(DS p.24–25, Table 7-3 and Table 7-4)

| Addr (hex) | Register Name | POR (hex) | POR (binary) | Type |
|------------|---------------|-----------|--------------|------|
| 0x00 | Configuration | 0x7127 | 01110001 00100111 | R/W |
| 0x01 | Channel-1 Shunt Voltage | 0x0000 | 00000000 00000000 | R |
| 0x02 | Channel-1 Bus Voltage | 0x0000 | 00000000 00000000 | R |
| 0x03 | Channel-2 Shunt Voltage | 0x0000 | 00000000 00000000 | R |
| 0x04 | Channel-2 Bus Voltage | 0x0000 | 00000000 00000000 | R |
| 0x05 | Channel-3 Shunt Voltage | 0x0000 | 00000000 00000000 | R |
| 0x06 | Channel-3 Bus Voltage | 0x0000 | 00000000 00000000 | R |
| 0x07 | Channel-1 Critical Alert Limit | 0x7FF8 | 01111111 11111000 | R/W |
| 0x08 | Channel-1 Warning Alert Limit | 0x7FF8 | 01111111 11111000 | R/W |
| 0x09 | Channel-2 Critical Alert Limit | 0x7FF8 | 01111111 11111000 | R/W |
| 0x0A | Channel-2 Warning Alert Limit | 0x7FF8 | 01111111 11111000 | R/W |
| 0x0B | Channel-3 Critical Alert Limit | 0x7FF8 | 01111111 11111000 | R/W |
| 0x0C | Channel-3 Warning Alert Limit | 0x7FF8 | 01111111 11111000 | R/W |
| 0x0D | Shunt-Voltage Sum | 0x0000 | 00000000 00000000 | R |
| 0x0E | Shunt-Voltage Sum Limit | 0x7FFE | 01111111 11111110 | R/W |
| 0x0F | Mask/Enable | 0x0002 | 00000000 00000010 | R/W |
| 0x10 | Power-Valid Upper Limit | 0x2710 | 00100111 00010000 | R/W |
| 0x11 | Power-Valid Lower Limit | 0x2328 | 00100011 00101000 | R/W |
| 0xFE | Manufacturer ID | 0x5449 | 01010100 01001001 | R |
| 0xFF | Die ID | 0x3220 | 00110010 00100000 | R |

**All registers are 16 bits wide.** All registers are volatile — reprogramming required after every power cycle. (DS p.15)

---

## 9. Detailed Register and Bitfield Breakdown

### 9.1 Configuration Register (0x00) [reset = 0x7127]

(DS p.26–27, Tables 7-5 and 7-6)

| Bit(s) | Field | R/W | Reset | Description |
|--------|-------|-----|-------|-------------|
| 15 | RST | R/W | 0 | System reset. Write 1 → POR-equivalent reset. Self-clears. |
| 14 | CH1en | R/W | 1 | Channel-1 enable. 0=disable, 1=enable. |
| 13 | CH2en | R/W | 1 | Channel-2 enable. 0=disable, 1=enable. |
| 12 | CH3en | R/W | 1 | Channel-3 enable. 0=disable, 1=enable. |
| 11–9 | AVG[2:0] | R/W | 0b000 | Averaging mode (number of samples). |
| 8–6 | VBUSCT[2:0] | R/W | 0b100 | Bus-voltage conversion time. |
| 5–3 | VSHCT[2:0] | R/W | 0b100 | Shunt-voltage conversion time. |
| 2–0 | MODE[3:1] | R/W | 0b111 | Operating mode. |

**AVG[2:0] — Averaging Mode Enumeration** (DS p.26):

| AVG[2:0] | Number of Averages |
|----------|--------------------|
| 000 | 1 (default) |
| 001 | 4 |
| 010 | 16 |
| 011 | 64 |
| 100 | 128 |
| 101 | 256 |
| 110 | 512 |
| 111 | 1024 |

**VBUSCT[2:0] / VSHCT[2:0] — Conversion Time Enumeration** (DS p.26):

| CT[2:0] | Conversion Time |
|---------|-----------------|
| 000 | 140 µs |
| 001 | 204 µs |
| 010 | 332 µs |
| 011 | 588 µs |
| 100 | 1.1 ms (default) |
| 101 | 2.116 ms |
| 110 | 4.156 ms |
| 111 | 8.244 ms |

**MODE[3:1] — Operating Mode Enumeration** (DS p.27):

| MODE[3:1] | Mode |
|-----------|------|
| 000 | Power-down |
| 001 | Shunt voltage, single-shot (triggered) |
| 010 | Bus voltage, single-shot (triggered) |
| 011 | Shunt and bus, single-shot (triggered) |
| 100 | Power-down |
| 101 | Shunt voltage, continuous |
| 110 | Bus voltage, continuous |
| 111 | Shunt and bus, continuous (default) |

**Important behavioral note**: Writing to the Configuration register halts any conversion in progress until the write completes, then a new conversion starts based on new contents. Reading the Configuration register does not affect settings or conversions. (DS p.26)

### 9.2 Shunt-Voltage Registers: Ch1 (0x01), Ch2 (0x03), Ch3 (0x05) [reset = 0x0000]

(DS p.28–30, Tables 7-7 through 7-16)

| Bit(s) | Field | R/W | Description |
|--------|-------|-----|-------------|
| 15 | SIGN | R | 0 = positive, 1 = negative (two's complement) |
| 14–3 | SD[11:0] | R | Shunt-voltage data (averaged) |
| 2–0 | Reserved | R | Always 0 |

- **Full-scale range**: 163.8 mV (register value 0x7FF8). (DS p.28)
- **LSB (SD0)**: **40 µV** per bit. (DS p.28)
- Data format: **two's complement**, sign-extended to 16 bits. Lower 3 bits always 0 (data in bits [15:3]). (DS p.28)
- These registers hold **averaged** values per the AVG setting. (DS p.24)

**Two's complement example** (DS p.28):
For VSHUNT = −80 mV:
1. |−80 mV| / 40 µV = 2000 decimal
2. 2000 = 0b 011 1110 1000 0 (in 13-bit + 3 reserved)
3. Complement: 0b 100 0001 0111 1
4. Add 1: 0b 100 0001 1000 0
5. Sign-extend to 16 bits: 0xC180

### 9.3 Bus-Voltage Registers: Ch1 (0x02), Ch2 (0x04), Ch3 (0x06) [reset = 0x0000]

(DS p.28–30, Tables 7-9 through 7-18)

| Bit(s) | Field | R/W | Description |
|--------|-------|-----|-------------|
| 15 | SIGN | R | 0 = positive, 1 = negative (two's complement) |
| 14–3 | BD[11:0] | R | Bus-voltage data |
| 2–0 | Reserved | R | Always 0 |

- **Full-scale range**: 32.76 V (register value 0x7FF8). **Do not apply more than 26 V**. (DS p.28)
- **LSB (BD0)**: **8 mV** per bit. (DS p.28)
- Data in bits [15:3], lower 3 bits reserved (always 0). Two's complement format. (DS p.28)

### 9.4 Critical-Alert Limit Registers: Ch1 (0x07), Ch2 (0x09), Ch3 (0x0B) [reset = 0x7FF8]

(DS p.31–32, Tables 7-19 through 7-28)

| Bit(s) | Field | R/W | Description |
|--------|-------|-----|-------------|
| 15–3 | CxL[12:0] | R/W | Critical-alert limit data bits (same scale as shunt voltage: 40 µV/LSB) |
| 2–0 | Reserved | R/W | Reserved |

- Default = 0x7FF8 = positive full-scale → alert effectively **disabled** at power-up. (DS p.12)
- Compared against **each single conversion** (not averaged). (DS p.12)

### 9.5 Warning-Alert Limit Registers: Ch1 (0x08), Ch2 (0x0A), Ch3 (0x0C) [reset = 0x7FF8]

(DS p.31–32, Tables 7-21 through 7-30)

| Bit(s) | Field | R/W | Description |
|--------|-------|-----|-------------|
| 15–3 | WxL[12:0] | R/W | Warning-alert limit data bits (same scale as shunt voltage: 40 µV/LSB) |
| 2–0 | Reserved | R/W | Reserved |

- Default = 0x7FF8 = positive full-scale → alert effectively **disabled** at power-up. (DS p.12)
- Compared against **averaged** shunt voltage value. (DS p.12)

### 9.6 Shunt-Voltage Sum Register (0x0D) [reset = 0x0000]

(DS p.33, Tables 7-31 and 7-32)

| Bit(s) | Field | R/W | Description |
|--------|-------|-----|-------------|
| 15 | SIGN | R | 0 = positive, 1 = negative (two's complement) |
| 14–1 | SV[13:0] | R | Shunt-voltage sum data bits |
| 0 | Reserved | R | Reserved |

- **LSB**: **40 µV**. (DS p.33)
- Updated with the most recent sum following each complete cycle of all selected channels. (DS p.33)
- Selected channels determined by SCC1–SCC3 bits in Mask/Enable register. (DS p.12, p.34)
- Contains sum of **single conversion** (un-averaged) shunt voltages of selected channels. (DS p.12)
- 15-bit data (sign + 14 data bits), bit 0 reserved. (DS p.33)

### 9.7 Shunt-Voltage Sum-Limit Register (0x0E) [reset = 0x7FFE]

(DS p.33, Tables 7-33 and 7-34)

| Bit(s) | Field | R/W | Description |
|--------|-------|-----|-------------|
| 15 | SIGN | R | Sign bit |
| 14–1 | SVL[13:0] | R/W | Sum-limit data bits |
| 0 | Reserved | R/W | Reserved |

- **LSB**: **40 µV**. (DS p.33)
- Default = 0x7FFE = positive full-scale → summation alert effectively disabled. (DS p.24)
- Compared against Shunt-Voltage Sum register after each complete cycle. (DS p.33)

### 9.8 Mask/Enable Register (0x0F) [reset = 0x0002]

(DS p.34–35, Tables 7-35 and 7-36)

| Bit | Field | R/W | Reset | Description |
|-----|-------|-----|-------|-------------|
| 15 | Reserved | R/W | 0 | Reserved |
| 14 | SCC1 | R/W | 0 | Summation channel control for Ch1. 0=disabled, 1=enabled. |
| 13 | SCC2 | R/W | 0 | Summation channel control for Ch2. 0=disabled, 1=enabled. |
| 12 | SCC3 | R/W | 0 | Summation channel control for Ch3. 0=disabled, 1=enabled. |
| 11 | WEN | R/W | 0 | Warning alert latch enable. 0=transparent, 1=latch enabled. |
| 10 | CEN | R/W | 0 | Critical alert latch enable. 0=transparent, 1=latch enabled. |
| 9 | CF1 | R/W | 0 | Critical-alert flag Ch1. Set if Ch1 exceeded critical limit. Cleared on read of this register. |
| 8 | CF2 | R/W | 0 | Critical-alert flag Ch2. Cleared on read. |
| 7 | CF3 | R/W | 0 | Critical-alert flag Ch3. Cleared on read. |
| 6 | SF | R/W | 0 | Summation-alert flag. Set if Shunt-Voltage Sum > Sum Limit. Asserts Critical pin. Cleared on read. |
| 5 | WF1 | R/W | 0 | Warning-alert flag Ch1. Cleared on read. |
| 4 | WF2 | R/W | 0 | Warning-alert flag Ch2. Cleared on read. |
| 3 | WF3 | R/W | 0 | Warning-alert flag Ch3. Cleared on read. |
| 2 | PVF | R/W | 0 | Power-valid flag. Mirrors PV pin state. Does NOT clear until condition is removed and PV pin clears. |
| 1 | TCF | R/W | 1 | Timing-control flag. Mirrors TC pin state. Does NOT clear after assertion unless power cycled or SW reset. Default=1 (high). |
| 0 | CVRF | R/W | 0 | Conversion ready flag. Set after ALL conversions complete. Cleared by: (1) writing Config register (except power-down/disable), or (2) reading Mask/Enable register. |

**CRITICAL behavioral notes**:
- Reading the Mask/Enable register **clears** CF1–CF3, SF, WF1–WF3, and CVRF. (DS p.34–35)
- Writing to Mask/Enable does **NOT** clear flag bits. (DS p.34)
- To avoid uncertainty: read Mask/Enable to clear flags **before** changing warning function settings. (DS p.34)
- PVF and TCF do NOT clear on read — they reflect pin state. (DS p.34–35)
- For SMBus Alert Response to function, alerts must be in **Latch mode** (WEN=1 and/or CEN=1). (DS p.23)

### 9.9 Power-Valid Upper-Limit Register (0x10) [reset = 0x2710]

(DS p.35, Tables 7-37 and 7-38)

| Bit(s) | Field | R/W | Reset | Description |
|--------|-------|-----|-------|-------------|
| 15 | SIGN | R | 0 | Sign bit (read-only) |
| 14–3 | PVU[11:0] | R/W | 0x4E2 | Power-valid upper-limit data bits |
| 2–0 | Reserved | R/W | 0 | Reserved |

- **LSB**: **8 mV**. (DS p.35)
- Default 0x2710 → decoded: bits [14:3] = 0x4E2 = 1250 decimal → 1250 × 8 mV = **10.000 V**. (DS p.35)
- PV pin asserts HIGH when ALL enabled bus-voltage channels exceed this limit. (DS p.13)

### 9.10 Power-Valid Lower-Limit Register (0x11) [reset = 0x2328]

(DS p.35–36, Tables 7-39 and 7-40)

| Bit(s) | Field | R/W | Reset | Description |
|--------|-------|-----|-------|-------------|
| 15 | SIGN | R | 0 | Sign bit (read-only) |
| 14–3 | PVL[11:0] | R/W | 0x465 | Power-valid lower-limit data bits |
| 2–0 | Reserved | R/W | 0 | Reserved |

- **LSB**: **8 mV**. (DS p.35)
- Default 0x2328 → bits [14:3] = 0x465 = 1125 decimal → 1125 × 8 mV = **9.000 V**. (DS p.35)
- If ANY bus-voltage channel drops below this limit (while PV condition was met), PV pin goes LOW. (DS p.13)

### 9.11 Manufacturer ID Register (0xFE) [reset = 0x5449]

(DS p.36, Tables 7-41 and 7-42)

| Bit(s) | Field | R/W | Reset | Description |
|--------|-------|-----|-------|-------------|
| 15–0 | D[15:0] | R | 0x5449 | "TI" in ASCII |

### 9.12 Die ID Register (0xFF) [reset = 0x3220]

(DS p.36, Tables 7-43 and 7-44)

| Bit(s) | Field | R/W | Reset | Description |
|--------|-------|-----|-------|-------------|
| 15–0 | D[15:0] | R | 0x3220 | INA3221 Die ID |

---

## 10. Commands and Transaction-Level Behaviors

### Register Pointer Mechanism (DS p.21)
- Every **write** operation begins with a register pointer byte after the target address byte.
- The register pointer value persists until changed by a new write operation.
- For **read**: the last-written register pointer determines which register is read. To change, issue a write with just the address + pointer byte (no data bytes needed).

### Write Word Transaction (DS p.21, Fig. 7-10)
```
[S] [ADDR+W] [ACK] [REG_PTR] [ACK] [DATA_MSB] [ACK] [DATA_LSB] [ACK] [P]
```
S = START, P = STOP, ACK = from device.

### Read Word Transaction (DS p.21–22, Fig. 7-9 + 7-11)
```
; Set pointer (if needed):
[S] [ADDR+W] [ACK] [REG_PTR] [ACK] [P or Sr]
; Read:
[S or Sr] [ADDR+R] [ACK] [DATA_MSB from device] [ACK by controller] [DATA_LSB from device] [NACK by controller] [P]
```
Sr = repeated START. Controller NACK on last byte, then STOP. Controller may also ACK last byte per DS note (DS p.22).

### SMBus Alert Response (DS p.23, Fig. 7-12)
```
[S] [0001100 + R] [ACK by device] [device's 7-bit address + direction bit from device] [NACK by controller] [P]
```
- Alert response address = `0x0C` (7-bit: 0001100). (DS p.23)
- If multiple targets alert simultaneously, bus arbitration applies. Losing device continues to hold alert line low. (DS p.23)
- **Alerts must be in Latch mode** (WEN=1 / CEN=1) for SMBus Alert Response to function. (DS p.23)

### SMBus Timeout (DS p.5–6)
- The INA3221 includes a **28 ms** (min) to **35 ms** (max) timeout on the interface.
- If SCL is held low for more than 28 ms, the interface resets to prevent bus lock-up. (DS p.6, p.20)

### Write Impact on Conversions
- Writing to the Configuration register **halts any in-progress conversion**, then starts a new conversion based on new settings. (DS p.26)
- Writing to alert limit registers has no described impact on in-progress conversions.
- Reading any register does not impact conversions. (DS p.26)

---

## 11. Initialization and Configuration Sequences

### Recommended Initialization Sequence

Based on DS p.11, p.15, p.24–27, p.37–38:

1. **Verify device identity**:
   - Read Manufacturer ID register (0xFE) → expect `0x5449`.
   - Read Die ID register (0xFF) → expect `0x3220`.

2. **Optional: software reset**:
   - Write `0x8000` to Configuration register (0x00) to set RST bit.
   - Wait briefly (bit is self-clearing). Re-read to confirm RST=0.

3. **Configure operating parameters** (write Configuration register 0x00):
   - Set channel enables (CH1en, CH2en, CH3en) — bits 14–12.
   - Set averaging mode (AVG[2:0]) — bits 11–9.
   - Set bus-voltage conversion time (VBUSCT[2:0]) — bits 8–6.
   - Set shunt-voltage conversion time (VSHCT[2:0]) — bits 5–3.
   - Set operating mode (MODE[3:1]) — bits 2–0.

4. **Configure alert thresholds** (if used):
   - Write Critical Alert Limit registers (0x07, 0x09, 0x0B) with desired shunt-voltage threshold values.
   - Write Warning Alert Limit registers (0x08, 0x0A, 0x0C) with desired averaged-threshold values.
   - Write Shunt-Voltage Sum Limit (0x0E) if summation monitoring is needed.
   - Write Power-Valid Upper Limit (0x10) and Lower Limit (0x11) if PV monitoring is needed.

5. **Configure Mask/Enable register** (0x0F):
   - Set SCC1–SCC3 (bits 14–12) to select which channels contribute to summation.
   - Set WEN (bit 11) and CEN (bit 10) for latch vs. transparent alert behavior.

6. **Begin monitoring**:
   - For continuous mode: device is already converting.
   - For single-shot mode: each write to Configuration register triggers one conversion cycle.

### Example: Design from DS p.38, Table 8-1

Configuration register for overcurrent monitoring:
- VS = 5 V, CH1 enabled only, shunt voltage continuous mode, AVG=1, critical alert limit = 80 mV.
- Config register value for 1.1 ms shunt CT: `0x4125` (DS p.38).
- Config register value for 588 µs shunt CT: `0x40DD` (DS p.38).
- Critical Alert Limit reg (0x07) = `0x07D0` (80 mV / 40 µV = 2000 → shifted left 3 → 0x07D0 [sic: actually 2000 << 3 = 16000 = 0x3E80; but DS states 7D0h]). **Note**: The DS p.38 states the limit register setting is `7D0h` for 80 mV. This is likely the value including the shift convention; see Section 18 for discussion.

---

## 12. Operating Modes and State Machine Behavior

### Mode Overview (DS p.11, p.27)

The INA3221 has three fundamental operating states:

1. **Continuous mode** (MODE = 101, 110, or 111):
   - Device continuously cycles through all enabled channels.
   - For MODE=111: shunt → bus for Ch1, then shunt → bus for Ch2, then shunt → bus for Ch3, repeat. (DS p.11)
   - For MODE=101: shunt only for each enabled channel, then repeat.
   - For MODE=110: bus only for each enabled channel, then repeat.
   - Disabled channels are skipped. (DS p.11)

2. **Single-shot (triggered) mode** (MODE = 001, 010, or 011):
   - Writing a single-shot mode to Configuration register triggers ONE complete cycle of all enabled channels. (DS p.11)
   - After completion, device enters power-down state. (DS p.11)
   - To trigger another conversion, write to Configuration register again (even if the mode value does not change). (DS p.11)
   - CVRF bit is set after all conversions complete. (DS p.11)
   - Registers can be read during and after the conversion. Data is from last completed conversion. (DS p.11)

3. **Power-down mode** (MODE = 000 or 100):
   - Quiescent current reduced to 0.5 µA typ. (DS p.6)
   - Input current turned off. (DS p.11)
   - Registers readable/writable. (DS p.11)
   - Recovery time: **40 µs**. (DS p.11)

### Conversion Sequence Detail (DS p.11, p.17)

For continuous shunt+bus mode (111) with all 3 channels enabled:
```
Shunt1 → Bus1 → Shunt2 → Bus2 → Shunt3 → Bus3 → Shunt1 → Bus1 → ...
```

Total cycle time = (VSHCT + VBUSCT) × number_of_enabled_channels. (DS p.17)

Example: VSHCT=332 µs, VBUSCT=332 µs, 3 channels → (332+332) × 3 = **1.992 ms** per cycle. (DS p.18)

### Conversion Ready Flag (CVRF) (DS p.11, p.35)

- Set after ALL enabled conversions complete (all channels, all signals per mode). (DS p.35)
- Cleared by:
  1. Writing to Configuration register (except power-down/disable mode). (DS p.35)
  2. Reading the Mask/Enable register. (DS p.35)
- Especially useful for coordinating single-shot conversions. (DS p.11)

---

## 13. Measurement / Data Path Behavior

### Shunt Voltage Measurement (DS p.11, p.28)
- Measured between IN+ and IN− for each channel.
- Differential measurement with respect to IN−. (DS p.11)
- Range: **−163.84 mV to +163.8 mV**. (DS p.5)
- Resolution: **13-bit ADC**, LSB = **40 µV**. (DS p.5, p.28)
- Result stored in two's complement format, sign-extended, bits [15:3]. Bits [2:0] = 0. (DS p.28)

### Bus Voltage Measurement (DS p.11, p.28)
- Measured from IN− pin to GND. (DS p.11)
- Range: **0 V to 26 V** (ADC full-scale = 32.76 V but do not apply > 26 V). (DS p.28)
- Resolution: **13-bit ADC**, LSB = **8 mV**. (DS p.5, p.28)
- Two's complement format, bits [15:3]. (DS p.28)

### Formulas

**Current calculation** (derived from DS p.11, p.28):
$$I_{LOAD} = \frac{V_{SHUNT}}{R_{SHUNT}} = \frac{\text{Shunt Register Value} \times 40\,\mu\text{V}}{R_{SHUNT}}$$

**Bus voltage calculation** (DS p.28):
$$V_{BUS} = \text{Bus Register Value} \times 8\,\text{mV}$$

Where "Register Value" = the signed integer from bits [15:3] (i.e., raw_register >> 3, sign-extended).

**Power calculation** (not done internally — must be computed in host):
$$P = V_{BUS} \times I_{LOAD} = V_{BUS} \times \frac{V_{SHUNT}}{R_{SHUNT}}$$

Note: Unlike INA226/INA233, the INA3221 does **not** have internal power or current registers. (DS p.10, confirmed by app note #4 SBOA194 p.2: "monitors bus and shunt voltages" — no power accumulator)

### Averaging Behavior (DS p.16, Fig. 7-5)

The averaging function uses a **recursive (exponential-moving-average-like)** algorithm, not a simple arithmetic mean:

1. New sample measured.
2. Difference = New_Sample − Previous_Output_Register_Value.
3. Increment = Difference / AVG_number.
4. New_Output = Previous_Output + Increment.

The output register is updated after each new conversion on the corresponding channel. (DS p.12, p.16)

The larger the AVG number, the less influence a single new sample has → acts as a low-pass filter. (DS p.16)

**Important**: The critical-alert function compares each **single conversion** (before averaging), while the warning-alert function compares the **averaged** value. (DS p.12)

### Data Register Conversion

To convert raw register value to physical value:

```
raw_signed = (int16_t)(register_value)  // Already two's complement
data_value = raw_signed >> 3            // Arithmetic right-shift to get signed 13-bit value

For shunt: voltage_mV = data_value * 0.04   // 40 µV = 0.04 mV per LSB
For bus:   voltage_V  = data_value * 0.008   // 8 mV per LSB

Alternatively: voltage = (float)(register_value) / 8.0 * LSB_value
  where register_value is treated as signed 16-bit and LSB_value = 40µV or 8mV
  (since data is in bits[15:3], dividing by 8 extracts the 13-bit value)
```

### Summation Channel (DS p.12, p.33)
- Sum register (0x0D) accumulates single-conversion shunt voltage values for channels selected by SCC1–SCC3. (DS p.12)
- Updated after each complete cycle of all selected channels. (DS p.33)
- 15-bit data (sign + 14 bits), LSB = 40 µV. (DS p.33)
- For meaningful summation: **use the same shunt-resistor value** on all included channels. Otherwise do NOT use this to add individual values to report total current. (DS p.12)

---

## 14. Interrupts, Alerts, Status, and Faults

### Alert Pin Summary

| Alert Pin | Trigger Source | Compared Against | Update Timing | Latch Control |
|-----------|---------------|------------------|---------------|---------------|
| Critical | Single shunt conversion exceeds per-channel critical limit **OR** shunt-voltage sum exceeds sum limit | Critical Alert Limit regs (0x07/09/0B) or Sum Limit (0x0E) | After each individual conversion | CEN (bit 10 of Mask/Enable) |
| Warning | Averaged shunt voltage exceeds per-channel warning limit | Warning Alert Limit regs (0x08/0A/0C) | After each averaging update | WEN (bit 11 of Mask/Enable) |
| PV | All bus voltages above upper limit → HIGH; any drops below lower limit → LOW | PV Upper (0x10) and Lower (0x11) Limit regs | After each bus-voltage measurement cycle | N/A (level-sensitive) |
| TC | Channel-2 bus voltage not reaching 1.2 V within ~28.6 ms after Ch1 reaches 1.2 V | Fixed 1.2 V threshold | Power-up / SW reset only | N/A (one-shot) |

### Critical Alert Detail (DS p.12)
- Each shunt-voltage single conversion compared against corresponding Critical Alert Limit register.
- If ANY channel exceeds its limit → Critical pin goes LOW.
- Read Mask/Enable register to check CF1, CF2, CF3 (bits 9, 8, 7) to identify source channel.
- Default critical limits = 0x7FF8 (positive full-scale) → effectively disabled. (DS p.12)

### Summation Alert Detail (DS p.12)
- When SCC1–SCC3 enable channels for summation, the sum of selected single-conversion shunt voltages is compared against Shunt-Voltage Sum Limit register (0x0E).
- If sum exceeds limit → Critical alert pin goes LOW.
- SF bit (bit 6 of Mask/Enable) indicates summation alert. (DS p.34)
- Critical pin can be asserted by EITHER individual critical limits OR summation limit. Read CF1–CF3 and SF to disambiguate. (DS p.12)

### Warning Alert Detail (DS p.12)
- Averaged shunt voltage compared against Warning Alert Limit register per channel.
- If ANY channel's averaged value exceeds its limit → Warning pin goes LOW.
- Read WF1, WF2, WF3 (bits 5, 4, 3) in Mask/Enable register to identify source.
- Default warning limits = 0x7FF8 → effectively disabled. (DS p.12)

### Alert Latch Modes (DS p.34)

| Mode | WEN/CEN | Behavior |
|------|---------|----------|
| Transparent (default) | 0 | Alert pin deasserts when condition clears |
| Latch | 1 | Alert pin remains asserted until Mask/Enable register is read |

- SMBus Alert Response requires Latch mode. (DS p.23)

### Power-Valid (PV) Alert (DS p.13–14)
- PV pin starts LOW at power-up. (DS p.13)
- Goes HIGH when ALL enabled bus-voltage channels exceed Power-Valid Upper Limit (default 10 V). (DS p.13)
- If any channel drops below Power-Valid Lower Limit (default 9 V), PV goes LOW. (DS p.13)
- **Hysteresis**: upper limit for assertion, lower limit for deassertion — window behavior. (DS p.13)
- If not all 3 channels used: connect unused IN− pin to a used channel. Float unused IN+. (DS p.13)
- Bus-voltage measurements must be enabled (MODE includes bus) for PV to work. (DS p.13)
- PVF bit (bit 2 of Mask/Enable) mirrors PV pin state. Does not clear until condition is removed. (DS p.34)
- PV output via VPU pin through pull-up resistor RPU. Can use voltage divider RDIV for level shifting. (DS p.14)

### Timing-Control (TC) Alert (DS p.14–15)
- **Power-up sequence monitoring only** (or after SW reset). (DS p.14–15)
- Default settings: continuous shunt+bus mode.
- Monitors Ch1 bus voltage for **1.2 V** threshold. (DS p.14)
- After 1.2 V detected on Ch1, monitors Ch2 bus voltage for 1.2 V within **~28.6 ms** (4 complete cycles of all 3 channels at default timing = 4 × [2.2ms × 3 channels + overhead]). (DS p.14)
- If Ch2 does not reach 1.2 V → TC pin goes LOW. (DS p.14)
- Time from Ch1 detection to TC assertion: ~28.6 ms window, with one Ch2 measurement at ~2.2 ms. (DS p.14)
- TCF bit (bit 1 of Mask/Enable) defaults to 1 (HIGH = no fault). Goes to 0 if TC alerts. (DS p.34)
- TCF does NOT clear after assertion unless power is recycled or SW reset issued. (DS p.35)
- Writing Configuration register before TC completes → TC function disabled until next power cycle / SW reset. (DS p.15)

### Conversion Ready Flag (CVRF) (DS p.35)
- Bit 0 of Mask/Enable register.
- Set after all enabled conversions complete.
- Cleared by: (1) writing Config register (except power-down), or (2) reading Mask/Enable register.
- Use for single-shot coordination. (DS p.11)

### Flag Clearing Summary

| Flag | Cleared by reading Mask/Enable? | Cleared by other means? |
|------|---------------------------------|-------------------------|
| CF1, CF2, CF3 | Yes | — |
| SF | Yes | — |
| WF1, WF2, WF3 | Yes | — |
| PVF | No (reflects pin state) | Clears when PV condition resolved |
| TCF | No | Only on power cycle or SW reset |
| CVRF | Yes | Writing Config register (except power-down) |

---

## 15. Nonvolatile Memory / OTP / EEPROM Behavior

**None.** All registers are volatile. Registers must be reprogrammed after every power cycle. (DS p.15)

---

## 16. Special Behaviors, Caveats, and Footnotes

1. **Bus voltage full-scale vs. actual limit**: ADC full-scale is 32.76 V but input pins must not exceed 26 V. (DS p.28)

2. **Averaging is NOT simple arithmetic mean**: It is a recursive (exponential-like) filter. New_Output = Old_Output + (New_Sample − Old_Output) / AVG. First samples after power-up/reset will ramp toward actual value. (DS p.16)

3. **Critical alert uses single conversions, Warning uses averaged values**: This is a deliberate design — critical catches transient spikes, warning catches sustained conditions. (DS p.12)

4. **Summation requires equal shunt resistors**: The sum register adds shunt voltages, not currents. For current summation to be meaningful, all channels must use the same R_SHUNT value. (DS p.12)

5. **PV function requires all 3 channels**: If fewer than 3 channels are used, unused IN− must be tied to a used channel's bus. (DS p.13)

6. **TC function timing depends on default settings**: Writing the Configuration register before TC completes disables TC. (DS p.15)

7. **Software reset holds PV pin**: PV output maintained during reset until power-valid sequence completes with (now-default) thresholds. (DS p.15)

8. **SMBus timeout resets interface**: SCL low for >28 ms resets the I2C state machine. This prevents bus lock-up but means firmware must not hold SCL low for extended periods. (DS p.6, p.20)

9. **Input protection**: For systems with potential inductive kickback or dV/dt events, add 10 Ω series resistors at each input. (DS p.19). From app note #5 (SBOA615): Input protection with RProtect + TVS/Zener recommended for EOS scenarios; limit input current to < 5 mA.

10. **Input filtering**: Only necessary for transients at harmonics of 500 kHz (±30%) internal sampling rate (≥ 1 MHz). Use ≤ 10 Ω + 0.1–1.0 µF ceramic. (DS p.19)

11. **No power/current registers**: Unlike INA226/INA233, the INA3221 does not compute power or current internally. Host must calculate from shunt voltage and known R_SHUNT. (DS p.10, confirmed SBOA194 p.2)

12. **A0 sampled on every communication**: Address is not latched at power-up; it is read dynamically. Ensure A0 is stable before I2C transactions. (DS p.20)

13. **Writing Config register halts active conversion**: A new conversion starts with the new settings after the write completes. (DS p.26)

14. **Kelvin connection recommended**: For shunt resistor connections to IN+ and IN−, use 4-wire Kelvin connection to avoid trace resistance errors. (DS p.39)

15. **Bypass capacitor**: 0.1 µF ceramic, placed as close as possible to VS and GND pins. (DS p.37–39)

16. **Thermal pad**: Connect to GND or leave floating. (DS p.3)

---

## 17. Recommended Polling and Control Strategy Hints from the Docs

### From the datasheet:

1. **Use CVRF for single-shot coordination** (DS p.11, p.35): After triggering a single-shot conversion, poll CVRF (bit 0 of Mask/Enable register, 0x0F) to know when data is ready. **Caveat**: reading Mask/Enable clears CVRF and all alert flags — read alert flags in the same read.

2. **Disable unused channels to improve response time** (DS p.17): Each disabled channel removes one conversion cycle's worth of delay before the device returns to measuring active channels.

3. **Use longer conversion times + averaging for accuracy** (DS p.18): Noise decreases with longer conversion times and higher averaging. Trade-off is slower update rate.

4. **Mix conversion times** (DS p.18): If bus voltage is relatively stable, use shorter VBUSCT and longer VSHCT to optimize timing. Example: VSHCT=4.156 ms + VBUSCT=588 µs for ~5 ms update per channel.

5. **For fastest single-channel monitoring** (DS p.17): Enable only one channel with one signal type → minimum latency between successive conversions.

6. **Use critical alert for fast overcurrent detection** (DS p.12): Critical alert compares every single conversion, providing faster response than the averaged warning alert.

7. **Alert handling strategy**:
   - Use transparent mode for auto-clearing real-time status. (DS p.34)
   - Use latch mode if alert must be held until firmware acknowledges it. (DS p.34)
   - Read Mask/Enable register to identify alert source AND to clear flags.
   - For SMBus Alert Response: use latch mode. (DS p.23)

### From application notes:

8. **Multi-rail POL monitoring** (app note #3, SBOA366C p.2): INA3221 frees up MCU ADC channels while using existing I2C bus. Suitable for monitoring multiple point-of-load supplies in telecom/server equipment.

9. **Power supply decoupling** (app note #5, SBOA615 p.6–7): Ensure GND pin has low-impedance direct path to decoupling cap. VS trace must route through the decoupling cap. Guard against EMI-induced latch-up with proper layout.

10. **Avoid floating VS pin** (app note #5, SBOA615 p.8–9): If power switching is needed, pull VS to GND with 5 kΩ resistor when supply FET is off. Never float VS while inputs are live.

---

## 18. Ambiguities, Conflicts, and Missing Information

1. **Critical Alert Limit register encoding vs. design example**: DS p.38 states critical alert limit for 80 mV = `7D0h`. But 80 mV / 40 µV = 2000 decimal. If the register format matches shunt voltage (data in bits [15:3], bits [2:0] = 0), then 2000 decimal in bits [15:3] → register value = 2000 << 3 = 16000 = `0x3E80`. However, the alert limit registers have bits [15:3] as data and bits [2:0] reserved, so the value `7D0h` = 2000 decimal placed directly, which would mean the register value is `0x7D0` × 8 = `0x3E80`... **Or** the DS may mean to write `0x7D0` as bits [15:3] (i.e. the whole 16-bit word is `0x7D00` shifted). The stated hex value `7D0h` is ambiguous as to whether it represents the raw 16-bit register word or the 13-bit data field. **Resolution needed**: Verify by testing or treating alert limit registers as having the same bit alignment as shunt voltage registers (data in [15:3]).

2. **Mask/Enable register RW behavior**: All bits are listed as R/W in the legend, but the flag bits (CF1–3, SF, WF1–3, TCF) are described as status indicators that are set by hardware. It is unclear if firmware can manually write 1 to these flags to force an alert condition, or if writes are silently ignored. The DS says "writing to this register does not clear the flag bit status" (DS p.34) but does not explicitly state whether writing 1 to a flag bit has any effect.

3. **Summation Sum-Limit register sign bit**: Table 7-34 shows SIGN bit (15) as R (read-only) with reset 0, and SVL[13:0] bits as R (read-only). Yet the register is listed as R/W in Table 7-3 and the bit field resets show "RW" prefixes. The register field description table on DS p.33 shows `R` for all fields, which may be a documentation error since the register is described as writable in the feature description (DS p.12) and the summary table (DS p.24).

4. **TC function: what happens if only 2 channels are used?**: DS describes TC monitoring Ch1→Ch2 sequencing. Behavior when Ch2 or Ch3 is disabled is not explicitly documented.

5. **TC function fixed thresholds**: The 1.2 V detection threshold and 28.6 ms timeout appear to be fixed and not programmable. This is implied but not explicitly stated as non-configurable. (DS p.14)

6. **Averaging startup transient**: The recursive averaging algorithm means initial readings ramp toward the true value. The number of conversions needed to converge is not specified. For AVG=1024, convergence is very slow. (DS p.16)

7. **No explicit register for reading unconverted/raw single-shot values**: The shunt-voltage output registers always contain the averaged value. The critical alert comparison uses the pre-averaged single conversion internally, but this raw value is not directly readable by the host. (DS p.12, p.16)

8. **Power-Valid with disabled channels**: DS says PV requires all three channels to reach the upper limit value. If a channel is disabled (CHxen=0), does PV still require that channel's IN− to exceed the threshold? DS p.13 says to tie unused IN− to a used channel but doesn't clarify if disabled-via-register channels are excluded from PV evaluation.

9. **Bus voltage sign bit**: Bus-voltage registers include a sign bit (bit 15) and are described as two's complement. However, bus voltage range is 0 V to 26 V (positive only). It is unclear under what conditions a negative bus voltage reading would occur or if this is just for format consistency.

---

## 19. Raw Implementation Checklist

- [ ] Set I2C address based on A0 pin hardware connection (0x40, 0x41, 0x42, or 0x43). (DS p.20)
- [ ] Verify device: read Manufacturer ID (0xFE) = 0x5449, Die ID (0xFF) = 0x3220. (DS p.36)
- [ ] Configure the device: write Configuration register (0x00) with desired channel enables, averaging, conversion times, and mode. (DS p.26–27)
- [ ] Handle 16-bit register reads: MSB first, LSB second. (DS p.21)
- [ ] Handle two's complement conversion for shunt and bus voltage registers. (DS p.28)
- [ ] Apply correct LSB scaling: shunt = 40 µV/bit, bus = 8 mV/bit, both in bits [15:3]. (DS p.28)
- [ ] Compute current in host: I = Vshunt / Rshunt. (DS p.11)
- [ ] Compute power in host: P = Vbus × I. (No internal power register.)
- [ ] For single-shot mode: write Config register to trigger, then poll CVRF in Mask/Enable (0x0F). (DS p.11, p.35)
- [ ] Reading Mask/Enable register clears CF/SF/WF/CVRF flags — capture all flags in one read. (DS p.34–35)
- [ ] Set critical-alert limits (0x07/09/0B) to desired thresholds (40 µV/LSB, data in bits [15:3]). (DS p.31)
- [ ] Set warning-alert limits (0x08/0A/0C) to desired thresholds (40 µV/LSB, data in bits [15:3]). (DS p.31–32)
- [ ] Set summation limits (0x0E) and enable SCC bits if using summation feature. (DS p.33–34)
- [ ] Set Power-Valid Upper (0x10) and Lower (0x11) limits if using PV feature (8 mV/LSB). (DS p.35)
- [ ] Configure latch mode (WEN/CEN in Mask/Enable) if using SMBus Alert Response. (DS p.23, p.34)
- [ ] Handle alert pins as open-drain: provide external pull-up resistors. (DS p.3, p.37)
- [ ] Handle PV pin: pulled up to VPU, active HIGH = valid. Optional RDIV for level shifting. (DS p.14)
- [ ] Handle TC pin: one-shot at power-up, active LOW = sequencing fault. (DS p.14–15)
- [ ] Software reset: write 0x8000 to Configuration register (0x00). Wait for self-clear. (DS p.15)
- [ ] Power-down mode: write MODE=000 or 100 to Config register. Recovery = 40 µs. (DS p.11)
- [ ] SMBus timeout awareness: do not hold SCL low > 28 ms. (DS p.6)
- [ ] Bypass VS with 0.1 µF ceramic cap close to pins. (DS p.37)
- [ ] Use Kelvin connections for shunt resistors. (DS p.39)
- [ ] For input filtering (if needed): ≤ 10 Ω + 0.1–1.0 µF ceramic at each channel input. (DS p.19)
- [ ] If using summation, ensure all included channels have equal R_SHUNT values. (DS p.12)
- [ ] If fewer than 3 channels used with PV: tie unused IN− to a used channel's bus. (DS p.13)

---

## 20. Source Citation Appendix

All citations use the format: `[Document abbreviation] p.[page number]`

| Abbreviation | Full Document | TI Document Number |
|-------------|---------------|--------------------|
| DS | INA3221 Datasheet, Rev. SBOS576C, Sep 2025 | SBOS576C |
| AN-CSA | Current Sense Amplifiers Guide 2021 | (no doc number) |
| AN-CSAPP | Current Sensing Applications in Communication Infrastructure Equipment | SBOA366C |
| AN-PEM | Power and Energy Monitoring With Digital Current Sensors | SBOA194 |
| AN-ESD | Risks and Prevention of ESD, EOS, and Latch Up Events for Current Sense Amplifiers | SBOA615 |
| REF-EBOOK | Current Sensing eBook (includes SLYA024A and others) | Multiple |

### Page-level citation index (key facts)

| Fact | Source |
|------|--------|
| Device description, features, package | DS p.1 |
| Pin configuration and functions | DS p.3 |
| Absolute maximum ratings | DS p.4 |
| ESD ratings | DS p.4 |
| Recommended operating conditions | DS p.4 |
| Electrical characteristics (full table) | DS p.5–6 |
| ADC conversion times | DS p.5 |
| SMBus timeout (28–35 ms) | DS p.5–6 |
| Basic ADC functions, measurement sequence | DS p.11 |
| Single-shot mode, power-down mode, CVRF | DS p.11 |
| Critical alert feature | DS p.12 |
| Summation control function | DS p.12 |
| Warning alert feature | DS p.12 |
| Power-valid alert, state diagram | DS p.13 |
| PV output structure | DS p.14 |
| Timing-control alert, timing diagram | DS p.14 |
| TC only at power-up or SW reset | DS p.15 |
| Software reset behavior | DS p.15 |
| Averaging function, block diagram | DS p.16 |
| Channel configuration, timing considerations | DS p.17–18 |
| Input filtering, ΔΣ ADC sampling rate | DS p.19 |
| I2C/SMBus bus overview | DS p.20 |
| Address table (A0 pin) | DS p.20 |
| Write/read timing diagrams | DS p.21–22 |
| High-speed I2C mode | DS p.22 |
| Bus timing table | DS p.23 |
| SMBus Alert Response | DS p.23 |
| Register map summary (Table 7-3) | DS p.24 |
| Register bit-map (Table 7-4) | DS p.25 |
| Configuration register details | DS p.26–27 |
| Shunt-voltage register details, two's complement example | DS p.28 |
| Bus-voltage register details | DS p.28–30 |
| Critical/Warning alert limit registers | DS p.31–32 |
| Shunt-voltage sum and sum-limit registers | DS p.33 |
| Mask/Enable register details | DS p.34–35 |
| Power-valid upper/lower limit registers | DS p.35–36 |
| Manufacturer ID and Die ID registers | DS p.36 |
| Typical application circuit | DS p.37 |
| Design example (80 mV critical alert) | DS p.38 |
| Power supply recommendations | DS p.38–39 |
| Layout guidelines and example | DS p.39 |
| Orderable information | DS p.42 |
| INA3221 as multi-channel POL monitor | AN-CSAPP p.2 |
| INA3221 vs. INA233 trade-offs | AN-PEM p.2 |
| ESD/EOS protection best practices | AN-ESD p.2–9 |
| INA3221 as 3-ch digital current/voltage monitor | REF-EBOOK (SLYA024A) p.14 |

---

## 21. Addendum — Missing Information Audit

The following subsections contain information found in the source PDFs that was absent or incomplete in the extraction above. Added per systematic page-by-page review.

### 21.1 Addendum: Thermal Resistance Values

(DS p.4, Table 6.4 "Thermal Information")

The following thermal metrics for the RGV (VQFN-16) package were not included:

| Thermal Metric | Symbol | Value | Unit |
|---------------|--------|-------|------|
| Junction-to-ambient thermal resistance | RθJA | 36.5 | °C/W |
| Junction-to-case (top) thermal resistance | RθJC(top) | 42.7 | °C/W |
| Junction-to-board thermal resistance | RθJB | 14.7 | °C/W |
| Junction-to-top characterization parameter | ψJT | 0.5 | °C/W |
| Junction-to-board characterization parameter | ψJB | 14.8 | °C/W |
| Junction-to-case (bottom) thermal resistance | RθJC(bot) | 3.3 | °C/W |

Reference: SPRA953 (Semiconductor and IC Package Thermal Metrics application report) for metric definitions. (DS p.5, footnote 1)

### 21.2 Addendum: INL (Integral Nonlinearity) Not Specified

The datasheet Electrical Characteristics table (DS p.5) specifies only **DNL** (±0.1 LSB max). **INL is not specified** for the INA3221. This is a notable omission compared to higher-precision devices like INA226. Firmware designers should not assume an INL value; if end-to-end linearity is critical, it must be characterized empirically.

### 21.3 Addendum: Quiescent Current Dependency on I2C Clock Frequency

(DS p.9, Figures 6-17 and 6-18)

The extraction lists ICC as 350 µA typ / 450 µA max (active) and 0.5 µA typ / 2 µA max (power-down) at TA=25°C. However, the datasheet typical-characteristics graphs reveal significant ICC dependency on I2C clock frequency:

**Active mode IQ vs. I2C clock frequency** (DS p.9, Fig. 6-17):
- ~350 µA at 10 kHz
- ~400 µA at 100 kHz
- ~480 µA at 400 kHz (fast mode)
- ~650 µA at 4 MHz (HS mode, estimated from curve)

**Shutdown mode IQ vs. I2C clock frequency** (DS p.9, Fig. 6-18):
- ~1 µA at low frequency
- ~50 µA at 400 kHz
- ~300 µA at 4 MHz (estimated from curve)

**Impact**: In high-speed I2C mode with frequent polling, the power consumption can nearly double compared to the tabulated typical value. Budget accordingly in power-sensitive designs. (DS p.9)

### 21.4 Addendum: Input Bias Current Variation with Common-Mode Voltage and Temperature

(DS p.8–9, Figures 6-12 through 6-14)

The extraction lists IIN+ = 10 µA typ and IIN− = 10 µA typ / 670 kΩ, measured at VIN+ = 12 V, TA = 25°C. The typical-characteristic curves reveal significant variation:

**IB vs. common-mode voltage** (DS p.8, Fig. 6-12, at TA=25°C):
- IB+ rises approximately linearly: ~5 µA at 0 V, ~10 µA at 12 V, ~45 µA at 26 V.
- IB− remains lower: ~2 µA at 0 V, ~10 µA at 12 V, ~25 µA at 26 V.

**IB vs. temperature** (DS p.9, Fig. 6-13, at VIN+=12 V):
- IB+ ranges from ~18 µA at −40°C to ~27 µA at +125°C.
- IB− ranges from ~3 µA at −40°C to ~10 µA at +125°C.

**IB in shutdown vs. temperature** (DS p.9, Fig. 6-14):
- Combined IB+ and IB− in shutdown: ~100 nA at −40°C, rising to ~450 nA at +125°C.

**Impact**: At high common-mode voltages (e.g., 26 V bus), total input bias current per channel can reach ~70 µA (IB+ + IB−), contributing additional measurement error proportional to the external source impedance. (DS p.8–9)

### 21.5 Addendum: All Channels Disabled Behavior

The datasheet does not explicitly document what happens when all three channels are disabled (CH1en = CH2en = CH3en = 0) while the device is in a continuous or single-shot mode (MODE ≠ 000/100). The DS states: "Any channels that are not enabled are bypassed in the measurement sequence, regardless of mode setting." (DS p.11)

By inference:
- With all channels disabled, there are no channels to sequence through.
- CVRF would presumably never be set since no conversions complete.
- Current consumption may not drop to power-down levels since MODE bits still indicate an active mode.
- **Recommendation**: To achieve minimum power, use MODE = 000 or 100 (power-down) rather than disabling all channels. This is an undocumented edge case.

### 21.6 Addendum: Shunt-Voltage Sum-Limit Register (0x0E) Field Type Clarification

(DS p.33, Tables 7-33 and 7-34)

The extraction's Section 18 item #3 notes a discrepancy. To fully document:

- **Table 7-3** (DS p.24): Lists register 0x0E as "R/W" type.
- **Table 7-33** (DS p.33): Bit-level reset annotations show `RW-0` and `RW-1` prefixes, confirming read/write.
- **Table 7-34** (DS p.33): Field descriptions list SIGN as "R" (read-only), SVL13-0 as "R" (read-only), and Reserved as "R" (read-only).

This is a confirmed **documentation error in Table 7-34**. The register IS writable (it is a user-programmable limit register). The field description table incorrectly shows "R" instead of "R/W" for the SVL data bits. The bit-level reset annotations and the register summary both confirm R/W. Firmware should treat bits [15:1] as writable, with bit 15 (SIGN) functionally R/W per the bit-level annotation (despite Table 7-38 style showing it as R for the Power-Valid registers). (DS p.33)

### 21.7 Addendum: I2C Bus Timing Table Footnote Discrepancy

(DS p.23, Table 7-2 footnote)

The footnote to Bus Timing Table 7-2 states: "A0 = A1 = 0." However, the INA3221 has **only one address pin (A0)**; there is no A1 pin. This appears to be a copy-paste error in the datasheet from a related device (e.g., INA226 which has A0 and A1). The timing values themselves are unaffected. (DS p.23)

### 21.8 Addendum: INA3221-Q1 Automotive Qualified Variant

(DS p.43, Package Option Addendum)

The datasheet notes: "OTHER QUALIFIED VERSIONS OF INA3221: Automotive: INA3221-Q1." This is a Q100-qualified variant for high-reliability automotive applications targeting zero defects. The base INA3221 extraction above covers the standard industrial variant only. For automotive designs, refer to the INA3221-Q1 specific datasheet. (DS p.43)

### 21.9 Addendum: Lead Finish and Packaging Details

(DS p.42, Package Option Addendum)

| Parameter | Value | Source |
|-----------|-------|--------|
| Lead finish | NIPDAU (NiPdAu) | DS p.42 |
| Package quantity per reel (SPQ) | 2500 | DS p.42, p.44 |
| Carrier type | Large T&R (Tape & Reel) | DS p.42 |
| Reel diameter | 330.0 mm | DS p.44 |
| Reel width (W1) | 12.4 mm | DS p.44 |
| Tape cavity A0 × B0 × K0 | 4.25 mm × 4.25 mm × 1.15 mm | DS p.44 |
| Tape pitch (P1) | 8.0 mm | DS p.44 |
| Tape width (W) | 12.0 mm | DS p.44 |
| Pin 1 quadrant in tape | Q2 | DS p.44 |
| Box dimensions (L × W × H) | 346.0 mm × 346.0 mm × 33.0 mm | DS p.45 |

### 21.10 Addendum: Channel-2 Shunt-Voltage Register Description Anomaly

(DS p.29)

The Channel-2 Shunt-Voltage Register description (address 03h) contains the statement: "Although the input range is 26V, the full-scale range of the ADC scaling is 32.76V. Do not apply more than 26V." This text applies to **bus-voltage** registers, not shunt-voltage registers (shunt full-scale is 163.8 mV). This appears to be a copy-paste error in the datasheet. The same incorrect text appears in the Channel-2 Bus-Voltage Register on the same page, where it is correct. (DS p.29)

### 21.11 Addendum: Digital Input Hysteresis Clarification

(DS p.5)

The extraction lists "Hysteresis voltage = 500 mV" in the electrical characteristics but does not specify what it applies to. This is the **digital input Schmitt trigger hysteresis** on the SDA and SCL pins. It provides noise immunity for digital bus communications. (DS p.5)

### 21.12 Addendum: EVM (Evaluation Module) Reference

(DS p.40)

TI provides an evaluation module for the INA3221:
- **EVM part number**: INA3221EVM
- **URL**: www.ti.com/tool/INA3221EVM
- **Related documentation**: INA3221EVM User Guide (DS p.40)

### 21.13 Addendum: Revision History Key Changes

(DS p.40–41)

Notable changes across datasheet revisions relevant to firmware:

**Rev A → Rev B (Sep 2014)**:
- Changed all VSENSE to VSHUNT throughout for consistency. (DS p.40)
- Changed "Status register" to "Mask/Enable register" to clarify register name. (DS p.40)
- Added Summation Control Function section. (DS p.41)
- Changed and added values in Bus Timing Table 7-2: added data valid time (t(VDDAT)), split fall time into separate data and clock times (tfDA, tfCL), deleted data rise time. (DS p.41)
- Changed D15 (SIGN) bit in Power-Valid Upper and Lower Limit registers to read-only. (DS p.41)
- Changed shunt voltage input range parameter values in Electrical Characteristics. (DS p.41)

**Rev B → Rev C (Sep 2025)**:
- No detailed changes listed (only header shown). (DS p.40)

### 21.14 Addendum: Typical Characteristics Figures Index

(DS p.7–9)

The datasheet contains the following typical-characteristic plots not described in the extraction above. All at TA=25°C, VS=3.3V, VIN+=12V unless noted:

| Figure | Title | Key Observation | Page |
|--------|-------|-----------------|------|
| 6-1 | Frequency Response | 3dB bandwidth visible; rolloff from ~100 Hz | DS p.7 |
| 6-2 | Shunt Input Offset Voltage Production Distribution | Gaussian centered near 0, range ±120 µV | DS p.7 |
| 6-3 | Shunt Input Offset Voltage vs. Temperature | ~30 µV at −50°C → ~48 µV at +150°C (typical) | DS p.7 |
| 6-4 | Shunt Input CMRR vs. Temperature | ~120 dB at 25°C, ~117 dB at +125°C | DS p.7 |
| 6-5 | Shunt Input Gain Error Production Distribution | Gaussian centered near 0, range ±0.3% | DS p.7 |
| 6-6 | Shunt Input Gain Error vs. Temperature | ~100 m% at −50°C → ~350 m% at +150°C | DS p.7 |
| 6-7 | Shunt Input Gain Error vs. Common-Mode Voltage | ~60 m% at 0V → ~175 m% at 26V | DS p.8 |
| 6-8 | Bus Input Offset Voltage Production Distribution | Gaussian centered near 0, range ±24 mV | DS p.8 |
| 6-9 | Bus Input Offset Voltage vs. Temperature | ~0 mV at −50°C → ~6 mV at +150°C | DS p.8 |
| 6-10 | Bus Input Gain Error Production Distribution | Gaussian centered near 0, range ±0.3% | DS p.8 |
| 6-11 | Bus Input Gain Error vs. Temperature | ~50 m% at −50°C → ~350 m% at +150°C | DS p.8 |
| 6-12 | Input Bias Current vs. Common-Mode Voltage | IB+ reaches ~45 µA at 26V | DS p.8 |
| 6-13 | Input Bias Current vs. Temperature | IB+ ~18–27 µA over temp range | DS p.9 |
| 6-14 | Input Bias Current vs. Temperature (Shutdown) | ~100–450 nA over temp | DS p.9 |
| 6-15 | Active IQ vs. Temperature | ~300 µA at −50°C → ~430 µA at +150°C | DS p.9 |
| 6-16 | Shutdown IQ vs. Temperature | ~0.3 µA at −50°C → ~3 µA at +150°C | DS p.9 |
| 6-17 | Active IQ vs. I2C Clock Frequency | ~350 µA at slow → ~650 µA at 4 MHz | DS p.9 |
| 6-18 | Shutdown IQ vs. I2C Clock Frequency | ~1 µA at slow → ~300 µA at 4 MHz | DS p.9 |
| 7-7 | Noise vs. Conversion Time | 140 µs CT has ~200 µV p-p noise; 8.244 ms CT has ~20 µV p-p | DS p.18 |

### 21.15 Addendum: ESD Handling Caution Statement

(DS p.40)

"This integrated circuit can be damaged by ESD. Texas Instruments recommends that all integrated circuits be handled with appropriate precautions. Failure to observe proper handling and installation procedures can cause damage. ESD damage can range from subtle performance degradation to complete device failure. Precision integrated circuits may be more susceptible to damage because very small parametric changes could cause the device not to meet its published specifications." (DS p.40)

### 21.16 Addendum: Multi-Channel Current Monitoring Trade-offs from eBook

(REF-EBOOK, SLYA024A p.14)

The current-sensing eBook provides a device comparison table with INA3221-specific trade-off:

| Device | Optimized Parameters | Performance Trade-off |
|--------|---------------------|-----------------------|
| INA4180 | 4-channel analog current monitor | Unidirectional measurement |
| INA4181 | Bidirectional 4-channel current monitor | Larger package |
| **INA3221** | **3-channel digital current/voltage monitor** | **No analog output** |
| INA2290 | 2-channel, 120-V analog current monitor | Unidirectional measurement |
| INA4290 | 4-channel, 120-V analog current monitor | Unidirectional measurement |

The "No analog output" trade-off means the INA3221 requires I2C communication for all data acquisition — there is no fast analog pin for use in closed-loop control. For applications requiring sub-millisecond current feedback (e.g., motor control), an analog-output CSA is preferred. (REF-EBOOK p.14)
