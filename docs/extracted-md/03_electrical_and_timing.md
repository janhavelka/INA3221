# INA3221 electrical and timing notes

## Operating limits relevant to software

| Parameter | Value | Source |
| --- | --- | --- |
| Recommended `VS` | 2.7 V to 5.5 V | Datasheet, pp. 3-4 |
| Operating temperature | -40 degC to +125 degC | Datasheet, pp. 1, 4 |
| Shunt input range | +/-163.84 mV | Datasheet, p. 5 |
| Bus input range | 0 V to 26 V | Datasheet, p. 5 |
| Fast-mode I2C | Up to 400 kHz | Datasheet, p. 20 |
| High-speed I2C | Up to 2.44 MHz | Datasheet, p. 20 |

## Measurement scaling

Source: INA3221 datasheet, pp. 5, 28-30.

| Measurement | Register scale |
| --- | --- |
| Channel shunt voltage | 40 uV/LSB; signed; full-scale code around `0x7FF8` represents +163.8 mV. |
| Channel bus voltage | 8 mV/LSB; bus ADC scaling extends beyond the 26 V input limit. |
| Shunt-voltage sum | 40 uV/LSB. |
| Power-valid upper/lower thresholds | 8 mV/LSB. |

## Conversion time and averaging

`CONFIG` controls both shunt and bus conversion times. Supported conversion times are:

| Field value | Time |
| --- | --- |
| `000b` | 140 us |
| `001b` | 204 us |
| `010b` | 332 us |
| `011b` | 588 us |
| `100b` | 1.1 ms |
| `101b` | 2.116 ms |
| `110b` | 4.156 ms |
| `111b` | 8.244 ms |

Averaging supports 1, 4, 16, 64, 128, 256, 512, or 1024 samples.

Source: INA3221 datasheet, pp. 16, 26-27.

## Timing implications

- Continuous shunt-and-bus mode updates each enabled channel after the selected shunt and bus conversions complete.
- Fewer enabled channels or shunt-only/bus-only modes reduce the time between updates for a given signal.
- Use `CVRF` in `Mask/Enable` to coordinate single-shot reads when conversion times or averaging are long.
