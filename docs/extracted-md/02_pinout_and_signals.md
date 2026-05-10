# INA3221 pinout and signals

Source: INA3221 datasheet, p. 3.

## RGV 16-pin VQFN pins

| Pin | Name | Type | Driver relevance |
| --- | --- | --- | --- |
| 1 | IN-3 | Analog input | Channel 3 load-side shunt input; bus voltage is measured from this pin to GND. |
| 2 | IN+3 | Analog input | Channel 3 supply-side shunt input. |
| 3 | GND | Ground | Device ground. |
| 4 | VS | Power | Device supply, 2.7 V to 5.5 V. |
| 5 | A0 | Digital input | I2C address strap; connect to GND, VS, SDA, or SCL. |
| 6 | SCL | Digital input | I2C/SMBus clock. |
| 7 | SDA | Digital I/O | I2C/SMBus data. |
| 8 | Warning | Digital output | Averaged measurement warning alert, open-drain. |
| 9 | Critical | Digital output | Conversion-triggered critical alert, open-drain. |
| 10 | PV | Digital output | Power-valid alert, open-drain. |
| 11 | IN-1 | Analog input | Channel 1 load-side shunt input; bus voltage is measured from this pin to GND. |
| 12 | IN+1 | Analog input | Channel 1 supply-side shunt input. |
| 13 | TC | Digital output | Timing-control alert, open-drain. |
| 14 | IN-2 | Analog input | Channel 2 load-side shunt input; bus voltage is measured from this pin to GND. |
| 15 | IN+2 | Analog input | Channel 2 supply-side shunt input. |
| 16 | VPU | Analog input | Pull-up supply voltage used to bias power-valid output circuitry. |
| Thermal pad | Thermal pad | Pad | Connect to GND or leave floating per datasheet. |

## Address selection

Source: INA3221 datasheet, p. 20.

| A0 connection | 7-bit address |
| --- | --- |
| GND | `0x40` |
| VS | `0x41` |
| SDA | `0x42` |
| SCL | `0x43` |

## Signal notes

- `SDA`, `SCL`, `Critical`, `Warning`, `PV`, and `TC` are open-drain style digital signals and require appropriate pullups.
- Shunt polarity is `IN+ - IN-` per channel.
- `IN-` is also the bus-voltage sense node for each channel.
