# INA3221 HIL Validation - COM5

- Started: `2026-06-30T15:48:43+02:00`
- Ended: `2026-06-30T15:52:05+02:00`
- Repository: `C:\Users\Honza\Documents\Projects\INA3221`
- Branch: `main`
- Commit: `859b93d157c049098854658e3bc63239f94a63a8`
- Dirty status: `M CHANGELOG.md
 M Doxyfile
 M README.md
 M examples/01_basic_bringup_cli/main.cpp
 M idf_component.yml
 M include/INA3221/INA3221.h
 M include/INA3221/Version.h
 M library.json
 M src/INA3221.cpp
 M test/test_basic.cpp
 M tools/hil_cli_runner.py
?? docs/reports/hil-validation-COM5-20260629-16h.md
?? docs/reports/hil-validation-COM5-20260629-16h.pid
?? docs/reports/hil-validation-COM5-20260629-16h.transcript.txt
?? docs/reports/hil-validation-COM5-20260630-preflight.transcript.txt`
- Host: `Windows-11-10.0.26200-SP0`
- Python: `3.12.10`
- PlatformIO: `PlatformIO Core, version 6.1.18`
- Serial: `COM5` at `115200` baud
- Per-command timeout: `30.0` s
- Idle timeout: `0.5` s
- Timeout resync: `10.0` s
- Boot settle: `3.0` s
- Command pacing: `250.0` ms
- Soak pacing: `250.0` ms
- Soak transcript capture: `enabled`

## Summary

| PASS | FAIL | UNKNOWN | NOT RUN |
|---:|---:|---:|---:|
| 66 | 0 | 0 | 0 |

## Boot Transcript

Raw transcript: `docs\reports\hil-validation-COM5-20260630-preflight.transcript.txt`

```text
ESP-ROM:esp32s3-20210327 Build:Mar 27 2021 rst:0x15 (USB_UART_CHIP_RESET),boot:0x8 (SPI_FAST_FLASH_BOOT) Saved PC:0x40379d02 SPIWP:0xee mode:DIO, clock div:1 load:0x3fce2820,len:0x118c load:0x403c8700,len:0x4 load:0x403c8704,len:0xc20 load:0x403cb700,len:0x30e0 entry 0x403c88b8 [I] === INA3221 Bringup Example === [I] I2C initialized (SDA=8, SCL=9) [I] Scanning I2C bus (timeout=50ms)... [I] 0 1 2 3 4 5 6 7 8 9 A B C D E F 00: -- -- -- -- -- -- -- -- 10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 20: 20 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 40: 40 41 -- -- -- -- -- -- -- -- -- -- -- -- -- -- 50: 50 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 70: -- -- -- -- -- -- -- -- [I] Scan complete. Found 4 device(s). [I] Checking INA3221 identity registers on 0x40-0x43... [I] 0x40: INA3221 recognized (A0=GND, mfg=0x5449, die=0x3220) [W] 0x41: responded, but not INA3221 (A0=VS, mfg=0x5449, die=0x2281) [I] INA3221 recognized: 1 device(s). [I] Common addresses: 0x40-0x43=INA3221, 0x48-0x4B=ADS1115, 0x51=RV3032, 0x76/0x77=BME280 [I] Device initialized successfully === Driver Healt...
```

## Detailed Results

| Test ID | Feature | Command | Expected | Observed | Elapsed s | Result | Notes |
|---|---|---|---|---|---:|---|---|
| CONN-001 | Serial CLI | version | Version Info, INA3221 library version | === Version Info === Example firmware build: Jun 30 2026 15:48:05 INA3221 library version: 2.0.0 INA3221 version code: 20000 (major=2 minor=0 patch=0) > | 0.031 | PASS | expected token(s) observed |
| CONN-002 | I2C discovery | scan | Scan complete, INA3221 recognized: | [I] Scanning I2C bus (timeout=50ms)... [I] 0 1 2 3 4 5 6 7 8 9 A B C D E F 00: -- -- -- -- -- -- -- -- 10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 20: 20 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 30: -- -- ... | 0.156 | PASS | expected token(s) observed |
| CONN-003 | Identity | probe | Status: OK | [I] Probing device (no health tracking)... Status: OK (code=0, detail=0) Message: OK > | 0.031 | PASS | expected token(s) observed |
| CONN-004 | Identity | ids | Manufacturer ID: 0x5449, Die ID: 0x3220 | Manufacturer ID: 0x5449 (OK - TI) Die ID: 0x3220 (OK - INA3221) > | 0.032 | PASS | expected token(s) observed |
| STATE-001 | Lifecycle/health | drv | Driver Health, State:, Total success | === Driver Health === State: READY Online: yes Consecutive failures: 0 Total success: 2 Total failures: 0 Success rate: 100.0% Last OK: 281 ms ago (at 4288 ms) Last error: never > | 0.031 | PASS | expected token(s) observed |
| STATE-002 | Settings/cache | settings | Cached Settings, Hardware config dirty | === Cached Settings === Initialized: YES State: READY Address: 0x40 I2C timeout: 50 ms Offline threshold: 5 Hooks: nowMs=YES yield=YES Mode: SHUNT_BUS_CONT Averaging: 1 samples VbusCT: 1100us VshCT: 1100us Channels: C... | 0.031 | PASS | expected token(s) observed |
| DATA-001 | Timing | timing | Timing Info, Cycle time | === Timing Info === Conversion time: 2200 us Cycle time: 6600 us Averaging: 1 samples VbusCT: 1100us VshCT: 1100us Shunt LSB: 0.04 mV (40 uV) Bus LSB: 8 mV Data shift: 3 bits (data in [15:3]) > | 0.031 | PASS | expected token(s) observed |
| DATA-002 | Config | config | Config:, Mode: | Config: 0x7127 Mode: SHUNT_BUS_CONT Averaging: 1 samples VbusCT: 1100us VshCT: 1100us CH1: ON CH2: ON CH3: ON Rshunt: CH1=0.1000 CH2=0.1000 CH3=0.1000 ohm Cycle time: 6600 us > | 0.032 | PASS | expected token(s) observed |
| DATA-003 | Aggregate read | read | CH1: | CH1: Vshunt=0.240 mV Vbus=5.040 V I=2.400 mA P=12.096 mW CH2: Vshunt=0.200 mV Vbus=5.024 V I=2.000 mA P=10.048 mW CH3: Vshunt=0.240 mV Vbus=5.032 V I=2.400 mA P=12.077 mW > | 0.031 | PASS | expected token(s) observed |
| DATA-004 | Channel read | ch 1 | CH1: | CH1: Vshunt=0.200 mV Vbus=5.040 V I=2.000 mA P=10.080 mW > | 0.031 | PASS | expected token(s) observed |
| DATA-005 | Raw shunt read | shuntraw 1 | CH1 shunt raw | CH1 shunt raw: 48 (0.240 mV) > | 0.031 | PASS | expected token(s) observed |
| DATA-006 | Raw bus read | busraw 1 | CH1 bus raw | CH1 bus raw: 5040 (5.040 V) > | 0.032 | PASS | expected token(s) observed |
| DATA-007 | Shunt float read | shunt 1 | CH1 shunt | CH1 shunt: 0.240 mV > | 0.031 | PASS | expected token(s) observed |
| DATA-008 | Bus float read | bus 1 | CH1 bus | CH1 bus: 5.040 V > | 0.031 | PASS | expected token(s) observed |
| DATA-009 | Current read | current 1 | CH1 current | CH1 current: 2.400 mA > | 0.031 | PASS | expected token(s) observed |
| DATA-010 | Power read | power 1 | CH1 power | CH1 power: 10.080 mW > | 0.032 | PASS | expected token(s) observed |
| DATA-011 | Shunt sum raw | sumraw | Shunt sum raw | Shunt sum raw: 0 > | 0.031 | PASS | expected token(s) observed |
| DATA-012 | Shunt sum float | sum | Shunt sum | Shunt sum: 0.000 mV > | 0.031 | PASS | expected token(s) observed |
| MODE-001 | Mode show | mode | Mode: | Mode: SHUNT_BUS_CONT > | 0.031 | PASS | expected token(s) observed |
| MODE-002 | Power-down mode | mode pd | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.032 | PASS | expected token(s) observed |
| MODE-003 | Continuous restore | mode sbc | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.031 | PASS | expected token(s) observed |
| MODE-004 | Triggered mode | mode sbtrig | Status: IN_PROGRESS | Status: IN_PROGRESS (code=11, detail=0) Message: Conversion started > | 0.031 | PASS | expected token(s) observed |
| MODE-005 | Triggered mode poll | poll | Conversion ready | [I] Conversion ready: YES > | 0.031 | PASS | expected token(s) observed |
| MODE-006 | Continuous restore | mode sbc | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.032 | PASS | expected token(s) observed |
| MODE-007 | Explicit triggered start | start sbtrig | Status: IN_PROGRESS | Status: IN_PROGRESS (code=11, detail=0) Message: Conversion started > | 0.031 | PASS | expected token(s) observed |
| MODE-008 | Triggered start poll | poll | Conversion ready | [I] Conversion ready: YES > | 0.031 | PASS | expected token(s) observed |
| MODE-009 | Continuous restore | mode sbc | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.031 | PASS | expected token(s) observed |
| CFG-001 | Averaging lower bound | avg 0 | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.032 | PASS | expected token(s) observed |
| CFG-002 | Averaging upper bound | avg 7 | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.031 | PASS | expected token(s) observed |
| CFG-003 | Averaging restore | avg 0 | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.031 | PASS | expected token(s) observed |
| CFG-004 | Bus CT lower bound | vbusct 0 | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.031 | PASS | expected token(s) observed |
| CFG-005 | Bus CT upper bound | vbusct 7 | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.032 | PASS | expected token(s) observed |
| CFG-006 | Bus CT restore | vbusct 4 | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.031 | PASS | expected token(s) observed |
| CFG-007 | Shunt CT lower bound | vshct 0 | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.031 | PASS | expected token(s) observed |
| CFG-008 | Shunt CT upper bound | vshct 7 | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.031 | PASS | expected token(s) observed |
| CFG-009 | Shunt CT restore | vshct 4 | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.032 | PASS | expected token(s) observed |
| CFG-010 | Disable channel | chen 3 0 | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.031 | PASS | expected token(s) observed |
| CFG-011 | Disabled-channel read rejection | ch 3 | Status: INVALID_CONFIG, Channel disabled | Status: INVALID_CONFIG (code=2, detail=0) Message: Channel disabled > | 0.031 | PASS | expected token(s) observed |
| CFG-012 | Restore channel | chen 3 1 | Status: OK | Status: OK (code=0, detail=0) Message: OK > | 0.031 | PASS | expected token(s) observed |
| ALERT-001 | Alert flags | alerts | Alert Flags | === Alert Flags === Critical: CH1=0 CH2=0 CH3=0 Warning: CH1=0 CH2=0 CH3=0 Summation=0 PowerValid=0 TimingCtl=1 ConvReady=1 > | 0.032 | PASS | expected token(s) observed |
| ALERT-002 | Mask/Enable decode | mask | Mask/Enable Register, clears latched alert | === Mask/Enable Register === Raw: 0x0003 Sum channels: CH1=OFF CH2=OFF CH3=OFF Latch: warning=OFF critical=OFF Critical flags: CH1=0 CH2=0 CH3=0 Warning flags: CH1=0 CH2=0 CH3=0 SF=0 PVF=0 TCF=1 CVRF=1 Note: reading t... | 0.031 | PASS | expected token(s) observed |
| ALERT-003 | Critical limits | crit | critical limit | CH1 critical limit: 32760 (163.800 mV) CH2 critical limit: 32760 (163.800 mV) CH3 critical limit: 32760 (163.800 mV) > | 0.031 | PASS | expected token(s) observed |
| ALERT-004 | Warning limits | warn | warning limit | CH1 warning limit: 32760 (163.800 mV) CH2 warning limit: 32760 (163.800 mV) CH3 warning limit: 32760 (163.800 mV) > | 0.031 | PASS | expected token(s) observed |
| ALERT-005 | Summation limit | sumlim | Shunt sum limit | Shunt sum limit: 32766 > | 0.032 | PASS | expected token(s) observed |
| ALERT-006 | Power-valid high | pvhi | Power valid upper limit | Power valid upper limit: 10000 (10.000 V) > | 0.031 | PASS | expected token(s) observed |
| ALERT-007 | Power-valid low | pvlo | Power valid lower limit | Power valid lower limit: 9000 (9.000 V) > | 0.031 | PASS | expected token(s) observed |
| ALERT-008 | Summation channels | sumch | Mask/Enable Register | === Mask/Enable Register === Raw: 0x0003 Sum channels: CH1=OFF CH2=OFF CH3=OFF Latch: warning=OFF critical=OFF Critical flags: CH1=0 CH2=0 CH3=0 Warning flags: CH1=0 CH2=0 CH3=0 SF=0 PVF=0 TCF=1 CVRF=1 Note: reading t... | 0.031 | PASS | expected token(s) observed |
| ALERT-009 | Alert latch | latch | Mask/Enable Register | === Mask/Enable Register === Raw: 0x0003 Sum channels: CH1=OFF CH2=OFF CH3=OFF Latch: warning=OFF critical=OFF Critical flags: CH1=0 CH2=0 CH3=0 Warning flags: CH1=0 CH2=0 CH3=0 SF=0 PVF=0 TCF=1 CVRF=1 Note: reading t... | 0.032 | PASS | expected token(s) observed |
| RESET-001 | Manual recovery | recover | Status: OK | [I] Attempting recovery... Status: OK (code=0, detail=0) Message: OK === Driver Health === State: READY Online: yes Consecutive failures: 0 Total success: 56 Total failures: 0 Success rate: 100.0% Last OK: 1 ms ago (a... | 0.031 | PASS | expected token(s) observed |
| RESET-002 | Software reset | reset | Status: OK | [I] Performing software reset... Status: OK (code=0, detail=0) Message: OK > | 0.031 | PASS | expected token(s) observed |
| RESET-003 | Post-reset recovery | recover | Status: OK | [I] Attempting recovery... Status: OK (code=0, detail=0) Message: OK === Driver Health === State: READY Online: yes Consecutive failures: 0 Total success: 61 Total failures: 0 Success rate: 100.0% Last OK: 0 ms ago (a... | 0.031 | PASS | expected token(s) observed |
| RESET-004 | Post-reset settings | settings | Cached Settings, Hardware config dirty: NO | === Cached Settings === Initialized: YES State: READY Address: 0x40 I2C timeout: 50 ms Offline threshold: 5 Hooks: nowMs=YES yield=YES Mode: SHUNT_BUS_CONT Averaging: 1 samples VbusCT: 1100us VshCT: 1100us Channels: C... | 0.032 | PASS | expected token(s) observed |
| MATH-001 | Shunt conversion | convert shunt -1 | Shunt raw -1 = | Shunt raw -1 = -0.040 mV > | 0.031 | PASS | expected token(s) observed |
| MATH-002 | Bus conversion high | convert bus 32767 | Bus raw 32767 = | Bus raw 32767 = 32.760 V > | 0.031 | PASS | expected token(s) observed |
| MATH-003 | Bus conversion negative | convert bus -1 | Bus raw -1 = | Bus raw -1 = -0.008 V > | 0.031 | PASS | expected token(s) observed |
| ERR-001 | Invalid command | unknown_hil_command | Unknown command | [W] Unknown command: unknown_hil_command > | 0.032 | PASS | expected token(s) observed |
| ERR-002 | Invalid channel | ch 4 | Invalid channel | [W] Invalid channel (1-3) > | 0.031 | PASS | expected token(s) observed |
| ERR-003 | Invalid averaging | avg 8 | Invalid avg | [W] Invalid avg (0-7) > | 0.031 | PASS | expected token(s) observed |
| ERR-004 | Invalid conversion time | vbusct x | Invalid conv time | [W] Invalid conv time (0-7) > | 0.031 | PASS | expected token(s) observed |
| ERR-005 | Invalid mode | mode nope | Invalid mode | [W] Invalid mode (pd/strig/btrig/sbtrig/sc/bc/sbc) > | 0.032 | PASS | expected token(s) observed |
| ERR-006 | Invalid register | reg 0x100 | Usage: reg | [W] Usage: reg <addr> > | 0.031 | PASS | expected token(s) observed |
| ERR-007 | Invalid stress count | stress 0 | Invalid count | [W] Invalid count (1-100000) > | 0.031 | PASS | expected token(s) observed |
| STRESS-001 | Self-test | selftest | INA3221 selftest, Selftest result:, fail=0 | === INA3221 selftest (safe commands) === [PASS] probe responds [PASS] probe no-health-side-effects [PASS] readConfig [PASS] readManufacturerId [PASS] readDieId [PASS] readChannel(CH1) [PASS] setMode(SHUNT_BUS_CONT) [P... | 0.031 | PASS | expected token(s) observed |
| STRESS-002 | Measurement stress | stress 5 | Stress Summary, Errors: 0 | === Stress Summary === Target: 5 Attempts: 5 Success: 5 Errors: 0 Duration: 7 ms Rate: 714.29 samples/s CH1 Vshunt mV: min=0.200 avg=0.200 max=0.200 CH1 Vbus V: min=5.040 avg=5.040 max=5.040 CH1 Current mA:min=2.000 a... | 0.032 | PASS | expected token(s) observed |
| STRESS-003 | Mixed stress | stress_mix 5 | stress_mix summary, fail=0 | === stress_mix summary === Total: ok=30 fail=0 (100.00%) Duration: 11 ms Rate: 2727.27 ops/s readBlocking ok=5 fail=0 readConfig ok=5 fail=0 mfgId ok=5 fail=0 shuntCh1 ok=5 fail=0 busCh2 ok=5 fail=0 alerts ok=5 fail=0... | 0.031 | PASS | expected token(s) observed |
| FINAL-001 | Final health | drv | Driver Health, State:, Total failures | === Driver Health === State: READY Online: yes Consecutive failures: 0 Total success: 157 Total failures: 0 Success rate: 100.0% Last OK: 269 ms ago (at 21454 ms) Last error: never > | 0.031 | PASS | expected token(s) observed |

## Soak Summary

- Result: `PASS`
- Start: `2026-06-30T15:49:05+02:00`
- End: `2026-06-30T15:52:05+02:00`
- Duration: `180.0` s
- Pass/fail/unknown: `339/0/0`
- Max consecutive failures: `0`
- Worst command latency: `0.047` s
- Worst read latency: `0.032` s
- Recover commands: `21`
- Stop reason: `completed requested duration`

| Command | Count |
|---|---:|
| `busraw 1` | 43 |
| `current 1` | 42 |
| `drv` | 21 |
| `poll` | 21 |
| `power 1` | 42 |
| `probe` | 21 |
| `read` | 43 |
| `recover` | 21 |
| `settings` | 21 |
| `shuntraw 1` | 43 |
| `stress_mix 5` | 21 |

## Limitations

- Electrical fault injection, disconnect testing, and unsafe stimulus are not attempted by this runner.
- Raw register writes are intentionally excluded from the default suite.
- Fixture-specific wiring and load plausibility require manual confirmation.
