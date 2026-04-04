# Power and Energy Monitoring With Digital Current Sensors
**Source:** power_energy_monitoring.pdf | **Doc #:** SBOA194 | **Pages:** 3

## Key Takeaways
- Digital power monitors (e.g., INA233) internally compute power from interleaved shunt and bus voltage measurements, minimizing time-alignment errors that plague ADC + processor approaches
- The 24-bit power accumulator enables on-demand average power and energy calculations without waiting for averaging intervals to complete
- ADC conversion times are programmable from 140 µs to 8.244 ms — longer times reduce noise; up to 1024 conversion cycles can be averaged
- The INA3221 is listed as an alternative device that trades accuracy/resolution for 3-channel monitoring capability
- Energy = Total_Accumulated_Power × ADC_conversion_time; use an external time base for accuracy since internal ADC timing can vary ±10%

## Summary
This application note explains why dedicated digital current monitors are superior to processor-based ADC approaches for power and energy monitoring. When a processor reads current and voltage via separate ADC channels, communication delays introduce time-alignment errors in the power calculation. Dynamic workloads make this worse since both current and voltage vary independently. Dedicating processor cycles to minimize this delay consumes resources needed for other tasks.

The INA233 solves this by internally computing power from interleaved shunt voltage and bus voltage measurements, independent of ADC conversion rates or digital bus communications. Its ALERT pin notifies the host processor only when current, power, or bus voltage exits the expected operating range. The 24-bit power accumulator continuously sums power readings, enabling average power and energy calculations on demand.

Average power is computed by dividing accumulated power by sample count. Energy is average power multiplied by the time interval. Since ADC conversion time has ±10% tolerance, an external time base is recommended. The accumulator must be read and cleared periodically to prevent overflow — overflow time depends on power level, ADC conversion time, and averaging settings.

## Technical Details

### Power Calculation Engine
- Shunt and bus voltage measured in interleaved fashion (not simultaneously, but back-to-back)
- Power = V_BUS × I_LOAD computed internally each conversion cycle
- Independent of I²C bus communication timing

### Key Formulas
$$\text{Average Power} = \frac{\sum_{i=1}^{n} \text{ADC\_Power\_Measurement}_i}{n}$$

$$\text{Energy} = \text{Average Power} \times \text{time} = \frac{\text{Total Accumulated Power} \times \text{ADC conversion time}}{1}$$

### ADC Configuration
| Parameter | Range |
|-----------|-------|
| Conversion time (shunt & bus) | 140 µs to 8.244 ms |
| Averaging | 1 to 1024 samples |
| Power accumulator | 24-bit (must be read before overflow) |

### Noise vs. Conversion Time
- 140 µs: highest noise, fastest update
- 1.1 ms: moderate noise
- 8.244 ms: lowest noise, slowest update

### Overflow Considerations
- Higher power → faster accumulator overflow
- Longer conversion times → slower overflow
- Low power + long conversion + high averaging → overflow can extend to hours/days
- Configure auto-clear after read to simplify host management

### Alternative Devices
| Device | Optimized For | Trade-off |
|--------|--------------|-----------|
| INA226 | I²C/SMBus, reduced register set | No power accumulator, no independent fault monitoring |
| INA231 | WCSP, lower cost | Less accuracy, no power accumulator |
| INA219 | Lowest cost | Less accuracy/resolution, no ALERT, no accumulator |
| **INA3221** | **3-channel monitoring** | **Less accuracy and resolution; monitors bus and shunt voltages** |

## Relevance to INA3221 Implementation
The INA3221 is explicitly listed as an alternative to the INA233 for applications requiring multichannel monitoring. Key differences: the INA3221 does **not** have a hardware power accumulator or on-chip power/energy calculation — these must be implemented in the driver software. The driver should:
1. Read shunt and bus voltage per channel and compute current and power in software
2. Implement software-side power accumulation and averaging if energy monitoring is needed
3. Minimize time between shunt and bus voltage reads per channel to reduce alignment error (or read the combined shunt+bus register in a single burst)
4. Expose programmable conversion time and averaging settings (the INA3221 has similar ADC configurability)
5. Utilize the INA3221's warning and critical alert thresholds for asynchronous fault notification, analogous to the INA233's ALERT pin
