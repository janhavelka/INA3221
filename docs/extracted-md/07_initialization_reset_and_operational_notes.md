# INA3221 initialization, reset, and operational notes

## Reset behavior

Power-on reset loads the register defaults in the register map. Writing `1` to `CONFIG.RST` performs a software reset equivalent to POR and returns volatile registers to defaults.

Source: INA3221 datasheet, pp. 14, 24, 26.

## Practical initialization sequence

1. Probe the selected 7-bit address from the `A0` strap.
2. Optionally read `Manufacturer ID` (`0x5449`) and `Die ID` (`0x3220`).
3. Write alert limits before enabling interrupt-dependent logic.
4. Configure `Mask/Enable` for latch behavior, enabled alert sources, and shunt-sum channel selection.
5. Configure `CONFIG` for enabled channels, averaging, conversion times, and operating mode.
6. In single-shot modes, write `CONFIG` to trigger a conversion and poll `CVRF` in `Mask/Enable`.
7. Read channel shunt and bus registers; convert raw values using 40 uV/LSB and 8 mV/LSB.

Source: INA3221 datasheet, pp. 11, 24-34.

## Operational notes

- Writes to `CONFIG` stop any current conversion until the write sequence completes, then conversions restart with the new settings.
- If some channels are unused, disable them in `CONFIG` and follow the datasheet guidance for power-valid behavior in unused channels.
- `PV` uses `VPU` for its output circuitry; board examples must wire the pin appropriately before treating it as a logic input.
- The device does not have a current register. Application code computes current as `shunt_voltage / shunt_resistance`.
