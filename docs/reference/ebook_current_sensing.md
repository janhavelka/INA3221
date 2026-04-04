# An Engineer's Guide to Current Sensing (e-book)
**Source:** ebook_current_sensing.pdf | **Doc #:** SLYY154B | **Pages:** 64

## Key Takeaways
- Comprehensive reference covering the full current sensing ecosystem: CSA fundamentals, signal chain integration, out-of-range detection, switching system measurements, extended common-mode techniques, and application-specific designs (BMS, robotics, USB-C PD, PoE)
- Shunt-based sensing dominates for currents up to ~100 A on rails below 100 V — smaller, more accurate, and more temperature-stable than magnetic solutions
- The INA3221 is recommended for multichannel POL monitoring, freeing controller ADC channels via I²C; INA226 is the single-channel precision benchmark (16-bit, 10 µV VOS, 0.1% gain error)
- For 20-bit precision in high-dynamic-range applications (e.g., EV BMS with 1 A to 1000 A range), the INA228/INA229 with 1 µV offset are the top-tier digital monitors
- Total measurement error follows RSS: $\sqrt{V_{OS}^2 + CMRR^2 + PSRR^2 + \text{GainError}^2 + \text{ShuntTolerance}^2 + \text{BiasCurrent}^2}$

## Summary

### 1. Current Sense Amplifier Fundamentals (Chapters 1–2)
CSAs are specialized differential amplifiers with matched internal gain resistors, supporting common-mode voltages far beyond their supply voltage. Key parameters are offset voltage (as low as 10 µV), gain (0.125–1000 V/V), gain error (as low as 0.01%), and temperature stability. The integrated gain network provides matching and drift performance impossible with discrete resistors. Four output types exist: analog voltage, digital (I²C/SPI), comparator (ALERT), and integrated shunt.

The signal chain path — shunt resistor → AFE (CSA) → ADC → controller — must be optimized by balancing shunt value (accuracy vs. power dissipation), amplifier gain (matching ADC full-scale range), and ADC resolution. The INA226 specializes this chain by integrating a 16-bit ADC with ±80 mV full-scale input range, providing 15× more resolution than a standard 16-bit ADC with 2.5 V full-scale range. Devices like INA250/INA260 further integrate the precision shunt resistor (2 mΩ, 0.1%, 15 ppm/°C), eliminating PCB parasitic resistance concerns.

Multichannel monitoring with INA2180/INA2181 (dual) or INA3221 (triple digital) reduces component count and improves matching. The INA2181's REF pin enables analog current summing or leakage detection between channels.

### 2. Out-of-Range Detection (Chapters 2–3)
Over-current protection ranges from simple fuses (slow, destructive) to integrated CSA-comparator solutions. The INA300 provides basic threshold comparison with 10 µs response. The INA301 integrates a CSA + comparator with < 1 µs alert response and 35 µV VOS. The INA302 provides dual thresholds (warning + fault) with adjustable delay from 3 µs to 10 s. The INA303 adds under-current detection and window-mode operation.

### 3. Application-Specific Designs (Chapters 3–8)

**BMS for HEV/EV:** Multi-decade current measurement challenge (1 A to > 1000 A). INA228/INA229 with 20-bit ADC and 1 µV offset on a 100 µΩ shunt achieves 1% error at 1 A and 312.5 nV LSB resolution. Zero-drift technology enables single-point calibration across temperature.

**Automotive 12-V Battery Monitoring:** Requires ≥ 40 V VCM survivability for load-dump protection. INA186-Q1 offers capacitively-coupled high-impedance input allowing 1 kΩ input filtering with minimal gain error. INA181-Q1 is cost-optimized but needs TVS diode input protection for VCM > 28 V.

**Battery Test Equipment:** Precision charging/discharging requires regulated current error of 0.1% and voltage error of 0.5%. Low-offset, low-drift amplifiers (TLV07, INA188) are critical. Type-III compensation needed for loop stability.

**Communication Infrastructure:** CSA needed at power supply, POL, and power amplifier stages. INA3221 recommended for multichannel POL monitoring. INA290 for high-voltage PA current sensing.

**PLC Digital Outputs:** INA240 provides low-offset (25 µV), high-BW (400 kHz) current measurement with PWM rejection for solenoid relay monitoring and over-current protection in industrial automation.

**USB Type-C PD:** 100 W power delivery requires accurate OCP. Discrete CSA (INA199: 1.5% accuracy at > 1 A) enables setting current limit at 4.926 A vs. 4.545 A with 10% integrated sensing — delivering 98.5 W vs. 90.9 W. INA381 integrates CSA + comparator for e-fuse implementation.

**Motor Current Sensing (In-Line):** INA240 with enhanced PWM rejection suppresses output glitches from common-mode voltage steps. Key specs: 25 µV VOS, 250 nV/°C drift, 0.2% gain error, 2.5 ppm/°C gain drift. INA253 adds integrated 2 mΩ precision shunt.

**Solenoid Control:** Solenoid impedance drifts ~40% over 100°C due to copper TCR. INA253 in closed-loop control reduces drift to < 0.2%. Common-mode drops one diode below ground on flyback — CSA must survive negative VCM.

**Switching Power Supplies:** Four measurement nodes in a buck converter have different VCM and transient characteristics. Peak current mode control (PCMC) uses inductor current directly as PWM comparator input — noise sensitive, benefits from INA240's high CMRR. Average current mode control (ACMC) adds gain/integration stage, improving noise immunity.

**Robotics:** Mobile robots (48 V Li-ion, BLDC motors) use INA241A for in-line phase current sensing and INA228 for battery monitoring. Industrial robots (400–600 V, 35–250 A) require isolated sensing (AMC1306). Collaborative robots (< 80 V, 10–30 A) use INA241A in constrained board space.

**PoE Systems:** Digital power monitors (INA219, INA238) or analog CSAs (INA180) on flyback converter primary/secondary help PDs stay within PoE class power limits. MCU throttles subsystems based on current data.

### 4. Extended Common-Mode and Isolation (Chapter 5)
Three techniques extend CSA operating range: resistor voltage dividers (simple but degrades CMRR/gain), floating ground reference with P-FET cascode (INA168 to 400 V), and mirrored sense voltage with INA226 for full power monitoring to 400 V. Isolated solutions (AMC1301, TMCS1100) provide reinforced isolation for high-voltage systems.

## Technical Details

### Signal Chain Design Rules
| Parameter | Guideline |
|-----------|-----------|
| Shunt selection | Minimize R_SHUNT to limit P = I²R; lower bound set by VOS/accuracy needs |
| Gain selection | Output at max current must not exceed ADC full-scale input |
| Kelvin connection | Required for shunts in low-mΩ range; eliminates PCB parasitic R |
| ADC conversion time | Longer → less noise; shorter → faster update rate |

### INA226 Key Specifications (Single-Channel Benchmark)
| Parameter | Value |
|-----------|-------|
| Full-scale input range | ±80 mV |
| LSB size | 2.5 µV |
| Max input offset voltage | 10 µV |
| Offset drift | 0.1 µV/°C |
| Max gain error | 0.1% |
| Common-mode range | 0–36 V |
| Supply | 2.7–5.5 V |
| Interface | I²C/SMBus |

### INA228/INA229 Key Specifications (20-bit Precision)
| Parameter | Value |
|-----------|-------|
| Full-scale input range | ±163.84 mV |
| LSB size | 312.5 nV |
| Max input offset voltage | ±1 µV |
| Common-mode range | 0–85 V |
| Resolution | 20-bit (1 sign + 19 data) |

### BMS Shunt Sizing Example (±1000 A)
| CSA | Gain | Full-Scale Input | Max Shunt | Offset Error at 1A | P_DIS at 1000A |
|-----|------|-----------------|-----------|--------------------|----|
| INA240A1 | 20 V/V | 250 mV | 125 µΩ | 20% | 125 W |
| INA240A4 | 200 V/V | 25 mV | 12.5 µΩ | 200% | 12.5 W |
| INA228 | ADC direct | 163.84 mV | 163.84 µΩ | 1% (at 100 µΩ) | 10 W |

### Total Measurement Error (RSS Method)
$$\text{Total Error} = \sqrt{V_{OS}^2 + CMRR^2 + PSRR^2 + \text{GainError}^2 + \text{ShuntTolerance}^2 + \text{BiasCurrent}^2}$$

### Multichannel Device Comparison
| Device | Channels | Output | VCM | Key Trade-off |
|--------|----------|--------|-----|---------------|
| INA2180 | 2 | Analog | −0.2 to 26 V | Unidirectional |
| INA2181 | 2 | Analog | −0.2 to 26 V | Bidirectional, REF pin for summing |
| INA4180 | 4 | Analog | −0.2 to 26 V | Unidirectional |
| INA4181 | 4 | Analog | −0.2 to 26 V | Bidirectional |
| **INA3221** | **3** | **I²C Digital** | **0 to 26 V** | **No analog output; lower accuracy** |
| INA2290 | 2 | Analog | 2.7 to 120 V | High-voltage, unidirectional |
| INA4290 | 4 | Analog | 2.7 to 120 V | High-voltage, unidirectional |

### Current Mode Control Comparison
| Method | Benefits | Drawbacks |
|--------|----------|-----------|
| Voltage Mode Control (VMC) | Simple architecture | Slow response; input voltage affects loop gain |
| Peak Current Mode Control (PCMC) | Fast response; pulse-by-pulse limiting | Noise sensitive; slope compensation needed > 50% duty |
| Average Current Mode Control (ACMC) | Improved noise immunity; no slope compensation | More complex; additional gain/integration stage |

## Relevance to INA3221 Implementation
The INA3221 appears throughout this ebook as the recommended multichannel digital monitor for POL applications. Key design insights for the INA3221 driver and system integration:

1. **Software power calculation:** Unlike INA226/INA228/INA233, the INA3221 lacks hardware power computation — the driver must multiply bus voltage × current per channel
2. **No power accumulator:** Energy monitoring must be implemented entirely in software with appropriate timing
3. **Multichannel summing/comparison:** The analog REF-pin current summing technique (INA2181) is unavailable; equivalent logic must be in the driver (e.g., sum currents across channels, detect leakage by comparing input vs. output current)
4. **Accuracy trade-off awareness:** The INA3221 has lower resolution and higher offset than INA226; for critical single-channel precision (< 0.1% error), consider pairing the INA3221 with an INA226 on the most critical rail
5. **Alert thresholds:** The INA3221's warning and critical alert registers map to the dual-threshold concept described in the INA302 section — the driver should expose and configure both for each channel
6. **Shunt sizing:** Apply the ebook's R_SHUNT selection methodology considering INA3221's full-scale shunt voltage range (±163 mV at gain = 1) and ADC resolution to determine minimum measurable current
7. **POL monitoring architecture:** The ebook's PoE and communication infrastructure chapters describe the exact use case — monitoring multiple DC/DC output rails and reporting to a host via I²C, with alerts for autonomous fault handling
