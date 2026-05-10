# INA3221 chip overview

The INA3221 is a three-channel current-shunt and bus-voltage monitor with an I2C/SMBus-compatible interface. Each channel reports shunt voltage and bus voltage. The device also provides programmable conversion time, averaging, channel enables, single-shot and continuous modes, and multiple alert outputs.

Source: INA3221 datasheet, pp. 1, 10.

## Driver-facing capabilities

| Capability | INA3221 facts to model | Source |
| --- | --- | --- |
| Three measurement channels | Channel 1, 2, and 3 each have separate shunt-voltage and bus-voltage registers. | Datasheet, pp. 1, 10, 24 |
| 13-bit style shunt/bus result fields in 16-bit registers | Shunt and bus values are left-aligned in bits 15:3; bits 2:0 are reserved. | Datasheet, pp. 24, 28-30 |
| Shunt voltage range | Measurement input range is +/-163.84 mV; LSB is 40 uV. | Datasheet, pp. 5, 28 |
| Bus voltage range | Bus input range is 0 V to 26 V; bus register LSB is 8 mV. | Datasheet, pp. 5, 28 |
| Alerts | Critical, Warning, Power Valid, and Timing Control pins are open-drain outputs. | Datasheet, pp. 3, 12-13 |
| Channel summation | Selected shunt channels can be summed and compared against a sum limit. | Datasheet, pp. 12, 32-33 |
| Four I2C addresses | One `A0` pin selects `0x40` through `0x43`. | Datasheet, p. 20 |

## Implementation facts tied to the datasheet

- All defined INA3221 registers are 16 bits and are transferred MSB first.
- INA3221 has no current or power result registers; current equals measured shunt voltage divided by the external shunt resistance.
- Critical, Warning, PV, and TC are open-drain outputs controlled by register flags and limits.
