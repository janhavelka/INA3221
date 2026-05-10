# INA3221 protocol, commands, and transactions

The INA3221 is an I2C/SMBus target device. It supports fast mode from 1 kHz to 400 kHz and high-speed mode from 1 kHz to 2.44 MHz. All data bytes are transmitted MSB first.

Source: INA3221 datasheet, pp. 20-23.

## Address byte

The 7-bit address is `10000 A0b`: `0x40` when `A0=GND`, `0x41` when `A0=VS`, `0x42` when `A0=SDA`, and `0x43` when `A0=SCL`. The R/W bit follows the 7-bit address. The device samples `A0` on every bus communication, and the pin state must be set before bus activity.

Source: INA3221 datasheet, p. 20.

## Register access model

| Operation | Bus sequence |
| --- | --- |
| Set pointer | START, address+W, pointer, STOP or repeated START |
| Write register | START, address+W, pointer, data MSB, data LSB, STOP |
| Read current pointer | START, address+R, data MSB, data LSB, STOP |
| Read specific register | START, address+W, pointer, repeated START, address+R, data MSB, data LSB, STOP |

All defined registers are 16 bits.

## SMBus alert response

The SMBus Alert Response address is `0001100b` with the R/W bit high. An alerting INA3221 acknowledges and sends `10000 A0 0b` as the response byte. If multiple devices respond, normal bus arbitration applies; the losing device does not acknowledge and keeps the alert line low until the interrupt is cleared. The datasheet states that alerts must be in latch mode for SMBus Alert Response to function.

Source: INA3221 datasheet, pp. 22-23.

## High-speed I2C

High-speed mode entry is part of the INA3221 bus protocol:

- While the bus is idle, the controller sends START plus high-speed controller code `00001XXXb`.
- That controller-code byte is sent in standard or fast mode at no more than 400 kHz.
- INA3221 does not acknowledge the controller code, but switches internal filters for 2.44 MHz operation after recognizing it.
- The controller sends a repeated START, then uses the same register format at up to 2.44 MHz.
- Repeated START conditions keep the device in high-speed mode; STOP ends high-speed mode and returns the filters to fast/standard mode.

Source: INA3221 datasheet, p. 22.

## Register transaction facts

- Every defined register is 16 bits.
- Every write operation requires a register pointer byte followed by data MSB and data LSB.
- A read uses the last pointer value written; repeated reads from the same register can omit the pointer write until the pointer is changed.
- A write to `CONFIG` halts any conversion in progress until the write completes, then conversion starts with the new settings.

Source: INA3221 datasheet, pp. 21-22, 24, 27.
