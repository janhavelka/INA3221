# INA3221 compact documentation inventory

This directory summarizes INA3221 Rev. C datasheet facts needed by the driver: three-channel measurement behavior, I2C address straps, SMBus transactions, 16-bit registers, reset values, alert flags, and shunt/bus scaling. The raw extraction archive remains in `docs/pdf-extracted-md/`.

| File | Purpose |
| --- | --- |
| `00_document_inventory.md` | Map of compact notes and source material. |
| `01_chip_overview.md` | Device role, channel model, and driver scope. |
| `02_pinout_and_signals.md` | Package pins, measurement inputs, address pin, and alert outputs. |
| `03_electrical_and_timing.md` | Electrical ranges, ADC scales, conversion time, averaging, and I2C timing. |
| `04_protocol_commands_and_transactions.md` | I2C/SMBus register pointer, read/write transactions, high-speed mode, and alert response. |
| `05_register_map.md` | Register summary, resets, and scaling notes. |
| `06_modes_interrupts_status_and_faults.md` | Conversion modes, alerts, mask/enable flags, and clear behavior. |
| `07_initialization_reset_and_operational_notes.md` | Startup sequence, reset, channel enabling, and conversion polling. |
| `08_variant_differences_and_open_questions.md` | INA3221 variant scope plus facts not documented or ambiguous in the checked-in PDFs. |

## Source documents

| Source PDF | Raw extract | Pages used | Notes |
| --- | --- | --- | --- |
| `docs/INA3221_datasheet.pdf` | `docs/pdf-extracted-md/INA3221_datasheet.md` | 1, 3-7, 10-24, 26-36 | Primary source for compact driver notes. |
| Supplemental current-sensing notes | `docs/pdf-extracted-md/*.md` except the datasheet | Not used for register facts | Background current-sensing material only; no INA3221 register defaults or I2C command facts were taken from these notes. |
