# INA3221 modes, interrupts, status, and faults

## Operating modes

`CONFIG.MODE` selects power-down, single-shot, or continuous conversion of shunt voltage, bus voltage, or both. Default reset mode is continuous shunt and bus conversion.

Source: INA3221 datasheet, pp. 11, 26-27.

| Mode bits | Mode |
| --- | --- |
| `000b` | Power-down |
| `001b` | Shunt voltage, single-shot |
| `010b` | Bus voltage, single-shot |
| `011b` | Shunt and bus, single-shot |
| `100b` | Power-down |
| `101b` | Shunt voltage, continuous |
| `110b` | Bus voltage, continuous |
| `111b` | Shunt and bus, continuous |

## Alert outputs

Source: INA3221 datasheet, pp. 12-13, 32-34.

| Pin | Behavior |
| --- | --- |
| `Critical` | Asserts when a single shunt conversion exceeds the per-channel critical limit or when selected shunt-channel sum exceeds the sum limit. |
| `Warning` | Asserts when averaged channel measurement exceeds the per-channel warning limit. |
| `PV` | Indicates bus-voltage rails are inside/outside programmed power-valid thresholds. |
| `TC` | Timing-control alert output. |

## Mask/Enable status and control

`Mask/Enable` (`0x0F`) controls shunt-sum channel selection, warning and critical latch enables, and exposes alert flags.

| Field group | Driver use |
| --- | --- |
| `SCC1..SCC3` | Select channels included in shunt-voltage sum. |
| `WEN`, `CEN` | Latch behavior for warning and critical alerts. |
| `CF1..CF3` | Per-channel critical flags. |
| `SF` | Summation alert flag. |
| `WF1..WF3` | Per-channel warning flags. |
| `PVF` | Power-valid flag. |
| `TCF` | Timing-control flag. |
| `CVRF` | Conversion-ready flag. |

Source: INA3221 datasheet, pp. 33-34.

## Clear behavior

- Critical and warning flag bits are cleared when `Mask/Enable` is read.
- `CVRF` is set after all conversions are complete and clears on `CONFIG` writes or `Mask/Enable` reads.
- Latched alert modes keep the output asserted until the flag clear behavior occurs and the condition no longer requires assertion.
