# INA3221 variants and source caveats

## Variant scope

The checked-in source is the INA3221 26 V triple-channel current and voltage monitor datasheet, Rev. C. These notes assume the 16-pin RGV VQFN package and the register map in that datasheet.

Source: INA3221 datasheet, pp. 1, 3, 24.

## Datasheet facts used by this repo

| Topic | Datasheet fact |
| --- | --- |
| Register width | All defined registers are 16 bits. |
| Byte order | Big-endian, MSB first. |
| Supported addresses | `0x40` to `0x43`, selected by `A0`. |
| Identity registers | `Manufacturer ID=0x5449`, `Die ID=0x3220`. |
| Measurement model | Per-channel shunt voltage and bus voltage; no built-in current/power result. |

## Not documented in PDFs

| Missing or ambiguous fact | Source status |
| --- | --- |
| Alternate package pinout | The checked-in INA3221 datasheet extract documents the RGV 16-pin VQFN pin table used in these notes. |
| Device behavior for reads from unlisted pointer addresses | The datasheet register summary lists defined addresses but does not specify read data for unlisted addresses. |
| A register containing calculated current or power | The INA3221 datasheet contains no current or power result register; current and power require host-side calculation from shunt voltage and the external shunt value. |
