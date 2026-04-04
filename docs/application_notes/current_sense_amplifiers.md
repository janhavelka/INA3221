# Current Sense Amplifiers Guide 2021# Current Sense Amplifiers Guide 2021



















































The INA3221 is a 3-channel digital power monitor with I²C interface, positioned in TI's portfolio as a multichannel solution for monitoring bus voltage and shunt voltage on up to 3 independent channels. It trades off single-channel accuracy (vs. INA226/INA228) for channel density. The guide's discussion of high-side vs. low-side measurement topologies directly applies to INA3221 channel configuration. The shunt resistor selection guidelines (balancing accuracy vs. power dissipation) and Kelvin connection requirements are essential for INA3221 PCB layout. The INA3221 driver should expose per-channel shunt voltage, bus voltage, and calculated current/power, mirroring the signal chain described in this guide.## Relevance to INA3221 Implementation- **In-line PWM sensing:** Use INA240/INA253 with enhanced PWM rejection; standard CSAs have limited AC CMRR- **Kelvin connection:** Required for low-ohmic shunts to eliminate PCB parasitic resistance errors- **Gain selection:** Match amplifier gain to ADC full-scale range at maximum expected current- **Shunt resistor tradeoff:** Larger R_SHUNT → better accuracy (offset less significant) but more power loss (P = I²R)### Design Rules| INA3221 | 0 to 26 V | I²C | 13-bit | 3-channel monitor || INA228 | 0 to 85 V | I²C | 20-bit | Energy accumulator || INA226 | 0 to 36 V | I²C/SMBus | 16-bit | Current, voltage, power ||--------|-----------|-----------|------------|----------|| Device | VCM Range | Interface | Resolution | Features |### Key Digital Power Monitor Specifications| INA290 | 2.7 to 120 V | ±100 µV | 0.02% | High-voltage, high-BW || INA186 | −0.1 to 40 V | ±50 µV | 0.02% | Capacitive-coupled input || INA240 | −4 to 80 V | ±25 µV | 0.05% | Enhanced PWM rejection || INA210 | −0.3 to 26 V | ±35 µV | 0.02% | Zero-drift || INA190 | −0.1 to 40 V | ±10 µV | 0.1% | 1.8 V, power down, nA bias ||--------|-----------|---------|---------------|----------|| Device | VCM Range | VOS Max | Gain Error Typ | Features |### Key Analog CSA Specifications| Integrated Shunt | On-chip precision sense resistor | INA250, INA253 || Comparator Output | ALERT signal on overcurrent threshold | INA300, INA301 || Digital Output | Integrated ADC with I²C/SMBus interface | INA226, INA228, INA3221 || Analog Output | Voltage/current output from matched gain network | INA190, INA210, INA240, INA186 ||------|------------|---------|| Type | Description | Example |### CSA Output Categories## Technical DetailsMultiple reference designs are cataloged, including automotive eFuse (TIDA-00795), 40–400 V power monitoring (TIDA-00528), three-phase motor current measurement (TIDA-00753), and 48 V GaN inverter with shunt-based sensing (TIDA-00913).The guide covers three measurement topologies: high-side (between supply and load), low-side (between load and ground), and in-line (series with each phase). Each topology has distinct advantages — high-side detects ground faults, low-side is simplest, and in-line provides the most accurate phase current feedback. The INA240 is highlighted for in-line PWM-driven applications due to its enhanced common-mode transient rejection.This guide provides a comprehensive overview of Texas Instruments' current sense amplifier portfolio. CSAs measure current by sensing the voltage drop across a shunt resistor, offering superior precision and noise immunity compared to discrete op-amp implementations. The integrated gain resistor network provides excellent matching and temperature drift stability that discrete solutions cannot achieve cost-effectively.## Summary- INA240 features enhanced PWM rejection for in-line motor current sensing with −4 V to +80 V common-mode range- High-side sensing detects load shorts to ground; low-side sensing is simpler/cheaper; in-line sensing gives best accuracy but faces PWM common-mode challenges- Key parameters for CSA selection: common-mode range, offset voltage (as low as 10 µV), gain (0.125–1000 V/V with errors as low as 0.01%), and temperature stability- Four CSA output categories exist: analog output, digital output (I²C), comparator output (ALERT), and integrated shunt — each suited to different system architectures- Current sense amplifiers (CSAs) are specialized differential amplifiers with precisely matched resistive gain networks, supporting currents from 10s of µA to 100s of A and common-mode voltages from −16 V to +80 V natively## Key Takeaways**Source:** current_sense_amplifiers.pdf | **Doc #:** TI Current Sense Amplifiers Guide 2021 | **Pages:** 8**Source:** current_sense_amplifiers.pdf | **Doc #:** TI Current Sense Amplifiers Guide 2021 | **Pages:** 8

## Key Takeaways
- Current sense amplifiers (CSAs) are specialized differential amplifiers with matched resistive gain networks, supporting currents from 10s of µA to 100s of A and common-mode voltages from -16 V to +80 V natively
- Four CSA output topologies exist: analog output, digital output (I2C), comparator output (ALERT), and integrated shunt — selection depends on system architecture
- High-side sensing detects load shorts to ground and provides high immunity to ground disturbance; low-side is simpler/cheaper; in-line gives best accuracy but faces PWM dV/dT challenges
- Key parameters for selection: common-mode range, offset voltage (as low as 10 µV), gain (0.125–1000 V/V, error as low as 0.01%), and temperature stability
- Integrated gain resistors offer superior matching, temperature drift, and board space vs. discrete op-amp implementations

## Summary
This guide provides an overview of TI's current sense amplifier portfolio, categorized by output type. Analog-output CSAs (e.g., INA190, INA210, INA240, INA199, INA180/181 families) provide voltage outputs with varying precision levels, common-mode ranges up to 120 V, and bandwidths up to 350 kHz. The INA240 family stands out for PWM-heavy applications with enhanced PWM rejection and -4 V to +80 V common-mode range.

Digital-output power monitors (e.g., INA226, INA228, INA3221) integrate ADC and I2C/SPI interfaces, performing on-chip current, voltage, and power calculations. These free the host processor from continuous ADC polling. The INA3221 monitors 3 channels simultaneously.

The guide covers three measurement topologies: high-side (between supply and load), low-side (between load and ground), and in-line (series with each phase). Each has distinct advantages for different applications. Several reference designs are presented including automotive eFuse (TIDA-00795), 40–400 V power monitoring (TIDA-00528), three-phase motor current (TIDA-00753), and 48 V GaN inverter with in-line sensing (TIDA-00913).

## Technical Details

| Parameter | Best-in-Class Value | Representative Device |
|---|---|---|
| Input Offset Voltage | ±10 µV max | INA190 |
| Offset Drift | 0.05 µV/°C typ | INA240, INA186 |
| Gain Error | 0.01% typ | INA216 |
| Common-Mode Range | -4 V to +120 V | INA290 |
| Bandwidth | 350 kHz | INA180/181 |
| Integrated Shunt | 2 mΩ, 0.1%, 15 ppm/°C | INA250, INA253 |

**Measurement topologies:**
- **High-side:** CSA between supply bus and load; requires high VCM capability; detects ground faults
- **Low-side:** CSA between load and ground; simple, low-cost; cannot detect load-to-ground shorts; disturbs system ground
- **In-line:** CSA in series with each phase; best for motor control; requires PWM rejection (INA240/INA253)

**Shunt resistor selection trade-off:**
- `V_shunt = I_load × R_shunt` — larger R_shunt improves SNR but increases power loss (`P = I² × R`)
- Offset voltage dominates error at low currents; gain error dominates at high currents

## Relevance to INA3221 Implementation
The INA3221 is listed as a digital-output power monitor capable of monitoring 3 independent channels with I2C interface. Key implementation considerations from this guide:
- The INA3221 performs high-side current sensing — shunt resistors connect between the supply bus and load on each channel
- Common-mode range of 0–26 V means the INA3221 is suited for point-of-load monitoring, not high-voltage bus sensing
- The trade-off noted in the product table: INA3221 provides "less accuracy and resolution" compared to single-channel devices like INA226, but monitors bus and shunt voltages across 3 channels
- Shunt resistor selection must balance offset error (dominant at low current) against power dissipation — the INA3221's 40 µV offset should be accounted for in the driver's error budget calculations
- For the software driver, calibration register programming and proper shunt value configuration are critical to accurate current/power readback
