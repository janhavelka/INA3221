# INA3221 register map

Source: INA3221 datasheet, pp. 24-36.

All INA3221 registers are 16 bits and are transferred MSB first.

| Address | Register | Access | Reset | Driver notes |
| --- | --- | --- | --- | --- |
| `0x00` | Configuration | R/W | `0x7127` | Reset, channel enables, averaging, conversion times, mode. |
| `0x01` | Channel-1 Shunt Voltage | R | `0x0000` | Signed shunt voltage, 40 uV/LSB. |
| `0x02` | Channel-1 Bus Voltage | R | `0x0000` | Bus voltage, 8 mV/LSB. |
| `0x03` | Channel-2 Shunt Voltage | R | `0x0000` | Signed shunt voltage, 40 uV/LSB. |
| `0x04` | Channel-2 Bus Voltage | R | `0x0000` | Bus voltage, 8 mV/LSB. |
| `0x05` | Channel-3 Shunt Voltage | R | `0x0000` | Signed shunt voltage, 40 uV/LSB. |
| `0x06` | Channel-3 Bus Voltage | R | `0x0000` | Bus voltage, 8 mV/LSB. |
| `0x07` | Channel-1 Critical Alert Limit | R/W | `0x7FF8` | Compare each conversion against limit. |
| `0x08` | Channel-1 Warning Alert Limit | R/W | `0x7FF8` | Compare averaged measurement against limit. |
| `0x09` | Channel-2 Critical Alert Limit | R/W | `0x7FF8` | Compare each conversion against limit. |
| `0x0A` | Channel-2 Warning Alert Limit | R/W | `0x7FF8` | Compare averaged measurement against limit. |
| `0x0B` | Channel-3 Critical Alert Limit | R/W | `0x7FF8` | Compare each conversion against limit. |
| `0x0C` | Channel-3 Warning Alert Limit | R/W | `0x7FF8` | Compare averaged measurement against limit. |
| `0x0D` | Shunt-Voltage Sum | R | `0x0000` | Sum of selected channel shunt conversions. |
| `0x0E` | Shunt-Voltage Sum Limit | R/W | `0x7FFE` | Critical alert source for summed current. |
| `0x0F` | Mask/Enable | R/W | `0x0002` | Alert enables, latches, flags, summation control, conversion-ready. |
| `0x10` | Power-Valid Upper Limit | R/W | `0x2710` | 8 mV/LSB; reset is 10.000 V. |
| `0x11` | Power-Valid Lower Limit | R/W | `0x2328` | 8 mV/LSB; reset is 9.000 V. |
| `0xFE` | Manufacturer ID | R | `0x5449` | TI ASCII identity. |
| `0xFF` | Die ID | R | `0x3220` | INA3221 identity. |

## Configuration register fields

| Bits | Field | Reset | Meaning |
| --- | --- | --- | --- |
| 15 | `RST` | 0 | Write 1 for software reset; self-clears. |
| 14:12 | `CH1en`, `CH2en`, `CH3en` | 111b | Per-channel enable bits. |
| 11:9 | `AVG` | 000b | 1 to 1024 samples. |
| 8:6 | `VBUSCT` | 100b | Bus conversion time. |
| 5:3 | `VSHCT` | 100b | Shunt conversion time. |
| 2:0 | `MODE` | 111b | Default continuous shunt and bus mode. |

Source: INA3221 datasheet, pp. 26-27.

## Documented reserved-bit behavior

| Register group | Reserved bits | Datasheet behavior |
| --- | --- | --- |
| Channel shunt registers `0x01`, `0x03`, `0x05` | Bits 2:0 | Reserved, read 0; shunt data is bits 15:3. |
| Channel bus registers `0x02`, `0x04`, `0x06` | Bits 2:0 | Reserved, read 0; bus data is bits 15:3. |
| Critical and warning limit registers `0x07`-`0x0C` | Bits 2:0 | Reserved, R/W reset 0; limit data is bits 15:3. |
| Shunt-voltage sum register `0x0D` | Bit 0 | Reserved, read 0. |
| Shunt-voltage sum limit register `0x0E` | Bit 0 | Reserved, read 0. |
| `Mask/Enable` register `0x0F` | Bit 15 | Reserved, R/W reset 0. |
| Power-valid limit registers `0x10`, `0x11` | Bits 2:0 | Reserved, R/W reset 0; threshold data is bits 14:3 plus sign bit. |

Source: INA3221 datasheet, pp. 28-36.
