# Current Sensing Applications in Communication Infrastructure Equipment
**Source:** current_sensing_application.pdf | **Doc #:** SBOA366C | **Pages:** 4

## Key Takeaways
- Wireless infrastructure (WI) equipment requires current sensing across three major domains: power supply block, point-of-load (POL), and power amplifiers (PA)
- The INA3221 is specifically recommended as a multichannel digital monitor for POL applications — frees up controller ADC channels while using an existing I²C bus
- POL CSA selection criteria: common-mode voltage, current range, accuracy, speed, and whether overcurrent protection (OCP) is needed
- PA current sensing is critical for bias control and overall energy efficiency — ~50% of total system power is consumed by the PA
- EZShunt™ technology (INA700, INA780A) integrates the shunt resistor into the device leadframe for ultra-compact solutions

## Summary
This application brief surveys current sensing needs across cellular wireless infrastructure equipment. The power supply block — fed from grid, solar, or battery — requires current/voltage sensing for intelligent power management and seamless source transitions. High-side sensing with CSAs like INA240/INA241A or galvanically isolated magnetic sensors (TMCS1123, TMCS1101) is preferred for fault-to-ground prevention.

At point-of-load, lower voltage rails derived from a 12–48 V bus need individual monitoring. Both analog and digital CSAs apply — analog multi-channel devices (INA4290, INA4180) provide four output channels, while the INA3221 digital monitor provides three independent channels of current, voltage, and power information over I²C with built-in warning and alert signals. This frees up scarce MCU ADC channels.

Power amplifier bias current varies with device tolerances and temperature. Accurate current sensing in the PA control loop is essential for bias optimization and energy efficiency management. Integrated PA monitor/control systems (AMC7836) or standalone high-voltage CSAs (INA290, INA281) address this need.

## Technical Details

### POL Current Sensing Architecture
- **High-side sensing:** CSA between supply bus and load; detects ground faults
- **Low-side sensing:** CSA between load and ground; simple but cannot detect ground shorts
- **Analog multi-channel:** INA4290 (4-ch, 120 V), INA4180 (4-ch, 26 V)
- **Digital multi-channel:** INA3221 (3-ch, I²C, warning/alert signals)

### EZShunt™ Integrated Shunt Devices
| Device | Package | Common-Mode | Shunt Resistance | Current Rating |
|--------|---------|-------------|------------------|---------------|
| INA700 | WCSP | 0–40 V | 2 mΩ | 10 A_RMS |
| INA780A | QFN | 0–85 V | 400 µΩ | 75 A_RMS |

### CSA Gain Options
- Fixed gain: 10 V/V to 1000 V/V (most CSAs)
- Configurable via pins: INA225 (digital control pins)
- Configurable via external resistor: INA139

### Alternate Device Recommendations
| Device | Key Feature |
|--------|------------|
| INA310A | −4 V to 110 V common-mode; integrated comparator |
| INA180 | Low I_Q; high bandwidth |
| INA190 | Low I_Q; low I_B; enable pin |
| INA301 | High bandwidth amplifier; fast comparator |

## Relevance to INA3221 Implementation
The INA3221 is directly recommended in this application note for POL monitoring in wireless infrastructure. Its three independent channels map naturally to monitoring multiple voltage rails (e.g., 3.3 V, 1.8 V, 1.2 V processor/FPGA rails) from a single device. The built-in warning and alert signals should be exposed in the driver for fast fault response. The driver should support reading both bus voltage and shunt voltage per channel, enabling current and power calculations. The note highlights that the INA3221 trades accuracy and resolution for multichannel capability compared to single-channel monitors like INA226 — the driver documentation should reflect these tradeoffs.
