# INA3221 Industry-Readiness Exploration

Date: 2026-05-31
Repository: `C:/Users/HonzovoSpectre/Documents/Projects/INA3221`
Branch: `audit/ina3221-industry-readiness-exploration`
Audit mode: deep exploration / no broad implementation

## Executive Summary

The library is much stronger than a typical hobby INA3221 driver: the core is framework-neutral, uses injected I2C callbacks, has a real `Status` model, checks manufacturer and die IDs, tracks health, latches offline state, supports the main INA3221 register set, and passes the available native and Arduino PlatformIO builds.

It is not industry-grade yet. The remaining blockers are not basic register constants; those mostly match the datasheet. The problems are production contracts around side effects, reset/recovery state, power-valid/timing-control alerts, timing semantics, test depth, and validation. Several APIs can silently clear alert flags by reading Mask/Enable, `begin()` can disable timing-control alert sequencing, reset/recover do not restore alert/PV limits, power-valid support is raw-register-only, and the timing model multiplies scan time by averaging count without datasheet proof.

## Readiness Classification

**Engineering-grade with major gaps**

The core architecture is good enough to harden incrementally. It is not blocked by framework ownership, heap-heavy design, or wrong basic register maps. It is blocked from production by alert side effects, incomplete reset/recovery cache semantics, unclear timing contracts, insufficient fault-injection tests, missing native `idf.py` CI/build proof, and no hardware validation evidence.

## Datasheet Sources Used

Primary local source:

- `docs/INA3221_datasheet.pdf` - INA3221 Rev. C datasheet, 50 pages.
- `docs/pdf-extracted-md/INA3221_datasheet.md` - extracted copy of the same PDF. The extract records Rev. C and SHA-256 metadata at line 5.

Datasheet sections/pages used:

- Electrical/safety: p.1, p.4-6; extracted lines 15, 203, 226, 278, 314, 344.
- Functional modes, power-down, single-shot, CVRF, summation, PV, TC: p.11-16; extracted lines 681-703, 734-740, 763-793, 861-904.
- Conversion timing and averaging response: p.17-19; extracted lines 952-985, 1017-1028.
- I2C/SMBus protocol, high-speed mode, alert response: p.20-23; extracted lines 1075-1198, 1253.
- Register map and field definitions: p.24-35; extracted lines 1273-1850.
- Bus/shunt scaling and raw format: p.28-31; extracted lines 1493-1606.
- Application/layout/Kelvin guidance: p.38-39; extracted lines 1905-2052.

Secondary local references reviewed:

- `docs/extracted-md/00_document_inventory.md` through `08_variant_differences_and_open_questions.md`
- `INA3221_triple_power_monitor_implementation_manual.md`
- `docs/IDF_PORT.md`
- `docs/IDF_PORT_IMPLEMENTATION.md`

## Repository Scope Reviewed

Reviewed:

- `include/INA3221/CommandTable.h`
- `include/INA3221/Config.h`
- `include/INA3221/INA3221.h`
- `include/INA3221/Status.h`
- `include/INA3221/Version.h`
- `src/INA3221.cpp`
- `test/test_basic.cpp`
- `test/stubs/Arduino.h`
- `test/stubs/Wire.h`
- `examples/01_basic_bringup_cli/main.cpp`
- `examples/common/*.h`
- `examples/esp_idf/basic/**`
- `platformio.ini`
- `library.json`
- `CMakeLists.txt`
- `idf_component.yml`
- `.github/workflows/ci.yml`
- `tools/check_core_timing_guard.py`
- `tools/check_cli_contract.py`
- `tools/check_idf_example_contract.py`
- `scripts/generate_version.py`
- `README.md`, `CHANGELOG.md`, `AGENTS.md`
- `docs/**`

Current file inventory highlights:

- Public headers only under `include/INA3221/`.
- Single core implementation file: `src/INA3221.cpp`.
- Arduino diagnostic CLI: `examples/01_basic_bringup_cli/`.
- Native ESP-IDF diagnostic CLI: `examples/esp_idf/basic/`.
- Arduino example glue: `examples/common/`.
- Native tests: `test/test_basic.cpp`.
- Existing ESP-IDF reports: `docs/IDF_PORT.md`, `docs/IDF_PORT_IMPLEMENTATION.md`.

## Architecture Scorecard

| Area | Rating | Notes |
| --- | --- | --- |
| Framework-neutral core | Strong | `include/` and `src/` include only project and standard headers; static scan found no Arduino, Wire, ESP-IDF, FreeRTOS, `String`, `Serial`, or framework delay/logging in core. |
| Injected I2C transport | Strong | `Config::i2cWrite` and `Config::i2cWriteRead` are required callbacks; core never owns `Wire` or IDF bus handles. |
| Status/error model | Good | Most fallible APIs return `Status`; gaps remain in `tick()`, `end()`, `conversionReady()`, invalid getters, and lack of explicit `OFFLINE` code. |
| INA3221 register correctness | Good | Register addresses, reset values, IDs, mode bits, channel bits, LSB constants, and MSB-first access match the datasheet. |
| Scaling/math correctness | Good | Sign extension and LSB conversion helpers look correct; raw APIs preserve register words and need clearer contract/tests. |
| Conversion timing semantics | Medium | Scan formula is mostly correct, but code treats averaging as exact CVRF latency without datasheet proof. |
| Alert/PV/TC semantics | Weak | Basic registers exist, but hidden Mask/Enable read clears, PV constraints, TC sequencing, and SMBus alert response are not production-safe. |
| Partial-state handling | Medium | Config setters roll back on write failure; reset/recover do not cache/reapply limit registers and have no dirty/sync-needed diagnostics. |
| Health/recovery | Good | Four-state health, offline latch, tracked wrappers, and manual `recover()` exist; some error classes are collapsed. |
| Thread/ISR contract | Weak | No documented external serialization; no locks/atomics; ISR safety is unsupported but not stated. |
| Tests/fault injection | Medium | 56 native tests pass, but fake I2C lacks sequencing/read-clear/fault-injection depth and many device-specific cases are missing. |
| Arduino examples | Good for diagnostics | Broad bring-up CLI; not production app code; Arduino transport ignores per-call timeout. |
| ESP-IDF port | Medium | Native IDF example and component metadata exist; no local or CI `idf.py` build proof; example has no bus lock. |
| Documentation honesty | Medium | README flags some limitations, but front-facing safety/PV/TC/summation/reset/timing warnings are incomplete. |
| Hardware validation | Unknown | No real hardware commands/logs were run or found. |

## What Is Already Strong

- Core framework boundary is clean: `include/INA3221/*.h` and `src/INA3221.cpp` avoid Arduino/ESP-IDF dependencies.
- I2C injection is non-owning and status-based: `Config.h:18`, `Config.h:30`, `src/INA3221.cpp:1228`, `src/INA3221.cpp:1240`.
- Identity verification exists in `begin()` through `probe()`: `src/INA3221.cpp:242`, `src/INA3221.cpp:297`.
- Basic register constants align with the datasheet: `CommandTable.h:14`, `CommandTable.h:39`, `CommandTable.h:56`, `CommandTable.h:82`.
- MSB-first register reads/writes are implemented correctly: `src/INA3221.cpp:1293`, `src/INA3221.cpp:1303`.
- Health wrappers centralize state updates: `src/INA3221.cpp:1244`, `src/INA3221.cpp:1257`, `src/INA3221.cpp:1326`.
- OFFLINE latches and normal public I2C calls stop touching the bus until `recover()`: `src/INA3221.cpp:1246`, `src/INA3221.cpp:1258`.
- Native ESP-IDF example uses `driver/i2c_master.h`, `app_main`, `esp_timer`, FreeRTOS waits, and fixed C buffers: `examples/esp_idf/basic/main/main.cpp:14`, `examples/esp_idf/basic/main/main.cpp:1356`.
- The available guard scripts, native tests, Arduino PlatformIO builds, and package step all pass in this audit environment.

## High-Severity Findings

### H1. Conversion readiness paths can silently clear alert flags

Severity: High

Evidence:

- `tick()` calls `_readConversionReadyAt()` and discards its status: `src/INA3221.cpp:260`, `src/INA3221.cpp:267`.
- `_readConversionReadyAt()` reads `REG_MASK_ENABLE`: `src/INA3221.cpp:1433`.
- `readConversionReady()`, triggered measurement readiness, and `readBlocking()` can also enter this path: `src/INA3221.cpp:635`, `src/INA3221.cpp:1458`, `src/INA3221.cpp:667`.
- Datasheet: reading Mask/Enable clears CVRF and alert flags; extracted lines 1749-1815.

Impact:

Applications can lose critical/warning/summation fault evidence before diagnostics call `readAlertFlags()`. This is a field-debugging and safety-monitoring failure mode.

Recommended remediation:

- Make every API that may read Mask/Enable explicitly side-effect-labeled.
- Consider separating "poll by time only" from "read hardware CVRF".
- Cache the full Mask/Enable snapshot when the driver reads it internally, or provide a side-effect report API so applications can retrieve flags cleared by internal polling.
- Change `tick()` to return `Status` or expose the last tick error and the last cleared flag snapshot.

Tests required:

- `test_tick_can_clear_mask_enable_flags_and_caches_or_reports_them`
- `test_read_conversion_ready_clears_fake_cvrf_and_alert_flags`
- `test_measurement_read_in_triggered_mode_documents_or_reports_alert_clear`
- Fake I2C must simulate Mask/Enable read-clear behavior.

### H2. Timing-control alert is unsafe under normal initialization

Severity: High

Evidence:

- `begin()` always applies configuration after probe: `src/INA3221.cpp:248`, `src/INA3221.cpp:1401`.
- Datasheet says writing Configuration before timing-control sequencing completes disables TC until power cycle or software reset; extracted line 861.
- The CLIs only expose TC through decoded Mask/Enable flags, not a TC sequencing workflow: `examples/01_basic_bringup_cli/main.cpp:1343`, `examples/esp_idf/basic/main/main.cpp:1203`.

Impact:

An application that expects TC alert sequencing can disable the feature just by calling `begin()`. The API currently lets users believe TC is supported because `AlertFlags::timingControl` exists, but no safe lifecycle is provided.

Recommended remediation:

- Add a documented TC mode/config option for "preserve TC sequencing until application releases it".
- Add explicit TC constraints to `begin()`, `softReset()`, README, and examples.
- Consider a `beginNoConfigWriteForTimingControl()` or `Config::preserveTimingControlSequence` if the feature is in scope.
- If not supported, mark TC as diagnostic flag only and document that normal `begin()` disables sequencing.

Tests required:

- Unit test that TC-preserve configuration avoids writing `REG_CONFIG` before app release.
- CLI/contract test for TC help text.
- Hardware validation with TC pin sequencing.

### H3. Power-valid support is raw and incomplete

Severity: High

Evidence:

- Driver exposes raw PV upper/lower setters only: `src/INA3221.cpp:1035`, `src/INA3221.cpp:1056`.
- No check that current mode includes bus measurement before setting/claiming PV support.
- No API contract for all-three-channel PV requirement or unused-channel wiring.
- Datasheet says PV requires all three channels and bus-voltage measurements; unused IN- must be tied to a used bus rail and unused IN+ floated; extracted lines 787-793 and 1817-1840.

Impact:

PV can be misleading in shunt-only modes or two-channel systems. A field system may treat PV as a valid rail-good signal when the hardware or mode cannot support it.

Recommended remediation:

- Add explicit PV contract docs in public headers and README.
- Provide PV configuration helpers that validate bus-measurement mode or return `INVALID_CONFIG`.
- Add a documented `PowerValidTopology` or equivalent if supporting fewer than three rails.
- Surface PV pin/VPU level-shift guidance in examples.

Tests required:

- `test_power_valid_requires_bus_mode_for_validated_helper`
- `test_power_valid_threshold_raw_masks_reserved_bits`
- `test_power_valid_defaults_match_10v_9v`
- Hardware PV pin tests with 3 rails and unused-channel wiring cases.

### H4. Reset/recover do not restore alert and PV limit state

Severity: High

Evidence:

- `softReset()` writes `RST`, resets cached config fields and `_maskEnableWritableCache`, but does not restore critical/warning/sum/PV limits: `src/INA3221.cpp:930`, `src/INA3221.cpp:951`.
- `recover()` reapplies cached config and Mask/Enable writable bits only: `src/INA3221.cpp:361`, `src/INA3221.cpp:367`.
- Limit setters write directly with no cache: `src/INA3221.cpp:960`, `src/INA3221.cpp:987`, `src/INA3221.cpp:1014`, `src/INA3221.cpp:1035`, `src/INA3221.cpp:1056`.
- Datasheet says software reset restores most registers and PV limits to defaults; extracted lines 871-878.

Impact:

After software reset, brownout, external reset, or manual recovery, custom alert/PV thresholds may be lost while the driver still looks operational. Overcurrent and rail-valid thresholds can silently revert to defaults.

Recommended remediation:

- Add caches for all writable non-config registers: critical limits, warning limits, shunt sum limit, PV upper/lower, Mask/Enable writable bits.
- Add an `applyAllSettings()` sequence with partial-failure tracking.
- Add `SettingsSnapshot` fields for cached limits and `dirty/syncNeeded` diagnostics.
- On `softReset()` and successful `recover()`, reapply all cached writable state unless configuration opts out.

Tests required:

- `test_soft_reset_reapplies_cached_alert_and_pv_limits`
- `test_recover_reapplies_all_cached_writable_registers`
- `test_partial_recover_failure_sets_sync_needed`
- Brownout/external reset hardware validation.

### H5. Conversion timing model over-specifies averaging latency

Severity: High

Evidence:

- `getCycleTimeUs()` multiplies conversion time by enabled channel count, enabled signal count, and averaging sample count: `src/INA3221.cpp:1211`.
- `_readConversionReadyAt()` gates hardware CVRF reads until this computed cycle expires: `src/INA3221.cpp:1421`.
- Tests assert `AVG_1024` multiplies cycle time: `test/test_basic.cpp:863`.
- Datasheet clearly defines scan time as enabled channels/signals, while averaging is described as response/filter behavior; extracted lines 952-956 and 980-985.

Impact:

For `3 channels * shunt+bus * 8.244 ms * AVG_1024`, the code can wait about 50.651 seconds before even checking CVRF. If hardware sets CVRF earlier, the driver introduces large deterministic latency. If hardware really requires AVG-scaled time, the current contract needs datasheet/hardware proof.

Recommended remediation:

- Split `getScanTimeUs()` from `getAveragedResponseTimeUs()`.
- Gate CVRF polling on scan time, not assumed averaged response time, unless TI/hardware validation proves otherwise.
- Document continuous reads as latest-sample reads, not fresh waits.
- Add hardware measurements for CVRF latency across averaging modes.

Tests required:

- `test_scan_time_formula_ignores_averaging`
- `test_averaged_response_time_reports_avg_scaled_estimate`
- Hardware CVRF timing matrix for all averaging values and representative conversion times.

## Medium-Severity Findings

### M1. Driver copy/move semantics are not disabled

Severity: Medium

Evidence:

- `INA3221` owns mutable config, health, and conversion state: `include/INA3221/INA3221.h:303`.
- No deleted copy/move constructors or assignment operators exist in `include/INA3221/INA3221.h`.
- Only internal `ScopedOfflineI2cAllowance` deletes copy operations: `src/INA3221.cpp:170`.

Impact:

Default copying can duplicate callback pointers, state counters, cached conversion state, and offline latch state. Two driver objects can then act on the same bus/device with divergent health.

Recommended remediation:

- Delete copy and move construction/assignment on `INA3221`.
- If move support is desired, implement it explicitly and document post-move state.

Tests required:

- `static_assert(!std::is_copy_constructible_v<INA3221::INA3221>)`
- Same for copy assignment, move construction, and move assignment.

### M2. Some public APIs hide failures or return sentinel values

Severity: Medium

Evidence:

- `tick()` returns `void` and discards readiness errors: `src/INA3221.cpp:260`.
- `end()` returns `void` and ignores best-effort power-down failure: `src/INA3221.cpp:271`.
- `conversionReady()` returns `false` on all errors: `src/INA3221.cpp:639`.
- `getChannelEnable()` returns `false` for invalid channels: `src/INA3221.cpp:847`.
- `getShuntResistance()` returns `0.0f` for invalid channels: `src/INA3221.cpp:867`.

Impact:

Field telemetry can lose the distinction between "not ready", "I2C failed", "offline", and "invalid argument".

Recommended remediation:

- Keep convenience APIs if needed, but add status-returning equivalents or document them as lossy.
- Consider `Status tick(uint32_t nowMs)` in the next breaking version.
- Add `getChannelEnable(Channel, bool&)` and `getShuntResistance(Channel, float&)`.

Tests required:

- `test_tick_ready_read_failure_reports_or_records_error`
- `test_conversion_ready_bool_false_on_error`
- Invalid-channel no-I2C tests for all getters.

### M3. Initial probe collapses transport failures into `DEVICE_NOT_FOUND`

Severity: Medium

Evidence:

- `probe()` maps raw ID read failures to `DEVICE_NOT_FOUND` except invalid config/param: `src/INA3221.cpp:299`, `src/INA3221.cpp:314`.
- Begin uses `probe()`: `src/INA3221.cpp:243`.

Impact:

Startup diagnostics lose address NACK vs timeout vs bus error class. Only `detail` survives, and that depends on the adapter.

Recommended remediation:

- Preserve the original transport `Err` in `Status::code` or add a nested diagnostic field.
- Alternatively return `DEVICE_NOT_FOUND` only for address NACK and preserve timeout/bus errors.

Tests required:

- `test_begin_preserves_timeout_on_mfg_read`
- `test_begin_preserves_bus_error_on_die_read`
- `test_probe_address_nack_reports_device_not_found_if_that_contract_is_kept`

### M4. Raw register write allows read-only addresses

Severity: Medium

Evidence:

- `isValidRegister()` accepts `0x00-0x11`, `0xFE`, and `0xFF`: `src/INA3221.cpp:52`.
- `writeRegister16()` uses the same validity check and can attempt writes to measurement and ID registers: `src/INA3221.cpp:1283`.

Impact:

Diagnostic code can issue nonsensical writes. This is less dangerous because it is explicit raw access, but it weakens API contracts and can obscure user mistakes.

Recommended remediation:

- Split `isReadableRegister()` and `isWritableRegister()`.
- Permit writes only to documented R/W registers.
- If intentional diagnostic writes are kept, rename to `writeRegister16Unsafe()`.

Tests required:

- `test_write_register_rejects_read_only_measurement_registers`
- `test_write_register_rejects_id_registers`
- `test_read_register_accepts_all_readable_registers`

### M5. Summation API does not guard unequal shunt resistors

Severity: Medium

Evidence:

- `setSummationChannels()` only accepts channel booleans: `src/INA3221.cpp:1111`.
- Datasheet says shunt-voltage summation is meaningful only if included channels use equal shunt resistors; extracted line 740.
- README does not surface this warning near summation APIs.

Impact:

Users can accidentally report "total current" from summed shunt voltage with unequal shunts, producing invalid numbers.

Recommended remediation:

- Add public-header and README warning.
- Add optional safe helper that refuses unequal configured shunts unless an override is passed.

Tests required:

- `test_safe_summation_rejects_unequal_shunts`
- `test_raw_summation_allows_advanced_use_with_warning_contract`

### M6. Default shunt resistance can produce plausible wrong current

Severity: Medium

Evidence:

- `Config` defaults all shunts to `0.1f` ohm: `include/INA3221/Config.h:116`.
- `readCurrent()` and `readChannel()` divide by cached shunt resistance: `src/INA3221.cpp:472`, `src/INA3221.cpp:530`.

Impact:

Current/power numbers look valid even if the user forgot to configure board-specific shunt values.

Recommended remediation:

- Consider requiring explicit shunt configuration before current/power APIs, or add a `Config::currentCalculationEnabled`/calibration flag.
- At minimum, strengthen README and examples around board-specific shunt values.

Tests required:

- `test_current_requires_explicit_shunt_if_contract_changes`
- Documentation guard/check for README shunt warning.

### M7. Thread-safety and ISR-safety contracts are missing

Severity: Medium

Evidence:

- Driver mutates `_config`, `_driverState`, counters, and conversion flags: `include/INA3221/INA3221.h:303`.
- No locks, atomics, critical-section hooks, or public serialization contract were found.
- Public methods call user I2C callbacks, floating-point math, and optional yield hooks.

Impact:

In multitask ESP-IDF systems, concurrent calls can corrupt driver state or interleave multi-step reads/writes. ISR use would be unsafe.

Recommended remediation:

- Document "not thread-safe; externally serialize all calls; not ISR-safe".
- Consider optional lock callbacks only if the core should own serialization.
- IDF example should show a bus/driver mutex if multitask use is demonstrated.

Tests required:

- Documentation contract check.
- Optional stress test with serialized vs unserialized task access on hardware/IDF.

### M8. ESP-IDF example transport has no bus lock and imprecise NACK mapping

Severity: Medium

Evidence:

- IDF adapter owns singleton `gTransport` and has no mutex/semaphore: `examples/esp_idf/basic/main/Ina3221IdfI2cTransport.cpp:13`.
- It maps `ESP_ERR_INVALID_RESPONSE` to generic `I2C_ERROR`, not address/data NACK: `examples/esp_idf/basic/main/Ina3221IdfI2cTransport.cpp:20`.
- Local docs acknowledge IDF master API phase limits: `docs/IDF_PORT.md:129`.

Impact:

Example is acceptable for single-task diagnostics but not a production multitask bus pattern. Error telemetry is less precise than the public status model suggests.

Recommended remediation:

- Add comments and README text that the example is single-owner/single-task glue.
- Add optional mutex in example or provide a production adapter sample.
- Keep `detail` as raw `esp_err_t` and document NACK phase limitation.

Tests required:

- Static contract test for no Arduino tokens remains.
- IDF host/build test when `idf.py` is available.
- Hardware or IDF integration test for timeout/NACK mapping.

### M9. Arduino transport ignores per-call timeout

Severity: Medium

Evidence:

- `examples/common/I2cTransport.h` discards `timeoutMs` in write/read paths: lines 40 and 90.
- Core passes `_config.i2cTimeoutMs` to callbacks: `src/INA3221.cpp:1228`, `src/INA3221.cpp:1240`.

Impact:

The core timeout contract depends entirely on adapter behavior. Arduino examples can still block according to framework defaults rather than the configured deadline.

Recommended remediation:

- Document adapter timeout limitations.
- If ESP32 Arduino supports bus timeout configuration safely in example glue, expose it outside the core.
- Add adapter tests/stubs for timeout propagation.

Tests required:

- `test_arduino_transport_receives_timeout_or_documents_ignore`
- Hardware stuck-bus timeout validation.

### M10. Test fake is too simple for production fault injection

Severity: Medium

Evidence:

- `FakeBus` exists in `test/test_basic.cpp:22`.
- `fakeWrite()` stores last write and ignores address/timeout: `test/test_basic.cpp:52`.
- `fakeWriteRead()` does not model read-clear side effects: `test/test_basic.cpp:67`.

Impact:

Many protocol bugs will not be caught: Mask/Enable read-clear, operation ordering, partial sequence failures, address-specific behavior, and timeout propagation.

Recommended remediation:

- Replace or extend `FakeBus` with queued transactions, address/timeout assertions, read-clear hooks, and per-transaction failure injection.

Tests required:

- `test_fake_bus_records_address_timeout_txrx_sequence`
- `test_fake_bus_supports_queued_read_write_failures`
- `test_fake_bus_simulates_mask_enable_read_clear`

## Low-Severity Findings

### L1. Front-facing README lacks key high-side safety warnings

Severity: Low

Evidence:

- Deeper docs mention 0 V to 26 V physical bus input limit and 32.76 V register scale: `INA3221_triple_power_monitor_implementation_manual.md:715`, `docs/extracted-md/03_electrical_and_timing.md:10`.
- README describes scaling and APIs but does not prominently warn about the 26 V physical limit.

Impact:

Users may confuse register full-scale with safe input range.

Recommended remediation:

- Add README safety section: 26 V max physical bus input, shunt differential limits, open-drain alert pins, VPU behavior, Kelvin routing, input filtering.

Tests required:

- Documentation review only.

### L2. Raw measurement API naming is easy to misread

Severity: Low

Evidence:

- `readShuntRaw()` and `readBusRaw()` return 16-bit register words with reserved low bits preserved: `src/INA3221.cpp:410`, `src/INA3221.cpp:431`.
- Conversion helpers then sign-extend bits `[15:3]`: `src/INA3221.cpp:1168`.

Impact:

Callers can treat raw values as shifted ADC counts and be off by 8x.

Recommended remediation:

- Document "raw register word" in public headers.
- Consider `readShuntRegisterRaw()` aliases or add `readShuntCounts()`.

Tests required:

- `test_read_shunt_raw_preserves_reserved_bits`
- `test_reserved_low_bits_do_not_affect_scaled_conversion`

### L3. Existing guards do not cover all framework leakage or heap patterns

Severity: Low

Evidence:

- `tools/check_core_timing_guard.py` checks timing calls and `Arduino.h` in core, but does not reject all forbidden framework tokens/dynamic containers.

Impact:

Future changes can accidentally introduce `Wire.h`, `String`, ESP-IDF headers, or heap use in core without guard failure.

Recommended remediation:

- Extend guard to reject `Wire.h`, `driver/`, `freertos/`, `String`, `Serial`, `TwoWire`, `new`, `malloc`, `std::vector`, and `std::string` in `include/` and `src/`.

Tests required:

- Guard script self-test or CI-only static check.

## INA3221 Device-Correctness Checklist

| Item | Status | Evidence / notes |
| --- | --- | --- |
| Address table | PASS | Code accepts `0x40`-`0x43`: `src/INA3221.cpp:13`; datasheet A0 table extracted line 1099. |
| Manufacturer/die ID | PASS | Constants `0x5449`, `0x3220`: `CommandTable.h:49`; checked in `probe()`: `src/INA3221.cpp:297`. |
| Configuration reset value | PASS | `CONFIG_DEFAULT = 0x7127`: `CommandTable.h:39`; datasheet Table 7-3 extracted line 1273. |
| Channel enable bits | PASS | Masks bits 14/13/12: `CommandTable.h:57`; build in `_buildConfigRegister()`: `src/INA3221.cpp:1481`. |
| Mode bits | PASS | Modes 0-7 in `Config.h:71` and `CommandTable.h:171`. |
| Conversion-time bits | PASS | Bus/shunt values 140 us through 8.244 ms: `CommandTable.h:151`, `CommandTable.h:162`. |
| Averaging bits | PASS | `AVG_1` through `AVG_1024`: `Config.h:47`, `CommandTable.h:140`. |
| Shunt raw scaling | PASS | 40 uV LSB, data in `[15:3]`: `CommandTable.h:125`, `src/INA3221.cpp:1168`. |
| Bus raw scaling | PASS | 8 mV LSB, data in `[15:3]`: `CommandTable.h:129`, `src/INA3221.cpp:1176`. |
| Reserved-bit masking | PARTIAL | Limit setters mask reserved bits; raw reads preserve them; tests need more negative/reserved cases. |
| Sign extension | PASS | Explicit `signExtendField()`: `src/INA3221.cpp:78`. |
| Single-shot trigger and re-trigger | PARTIAL | Config write triggers; driver blocks retrigger while in progress until ready observed. Needs more tests/hardware timing. |
| Continuous latest-sample semantics | PARTIAL | Code reads latest registers without waiting; needs explicit documentation. |
| CVRF semantics | PARTIAL | Reads CVRF and caches ready, but internal reads have hidden alert-clearing side effects. |
| Mask/Enable read-clears semantics | PARTIAL | Header notes explicit APIs; `tick()`/measurement side effects still weak. |
| Critical alert limits | PARTIAL | Raw setters/getters and reserved-bit masks exist; side-effect-safe alert workflow, mV helper, and hardware validation are missing. |
| Warning alert limits | PARTIAL | Raw setters/getters and reserved-bit masks exist; averaged-alert contract needs stronger docs/tests. |
| Summation control | PARTIAL | SCC bits supported; no equal-shunt guard/warning in public API. |
| Power-valid thresholds | PARTIAL | Raw PV registers supported; no bus-mode/all-three-channel/topology contract. |
| Timing-control alert | PARTIAL | TCF decoded; normal `begin()` can disable TC sequencing. |
| Software reset | PARTIAL | Reset command exists; alert/PV limit reset consequences are not handled. |
| SMBus alert response | NOT IMPLEMENTED | No alert-response address/API; latch mode helper exists. |
| SMBus timeout | NOT IMPLEMENTED | Left to bus/transport; docs mention 28 ms behavior. |
| High-speed I2C note | PARTIAL | Local docs mention 2.44 MHz/Hs sequence; core does not model protocol. |

## API and Contract Assessment

| API | Blocking? | I2C transactions | Side effects | Missing contract |
| --- | ---: | ---: | --- | --- |
| `begin(config)` | Yes | 3 on success | Resets driver runtime, raw ID reads, writes config, starts triggered conversion if configured | Begin-time transport errors collapsed by `probe()`; TC sequencing impact. |
| `tick(nowMs)` | Bounded | 0 or 1 | May read Mask/Enable and clear CVRF/alerts; discards error | Should report status/cleared flags or document side effect. |
| `end()` | Best effort | 0 or 1 | Writes power-down unless offline; clears state | Failure hidden. |
| `isInitialized()` | No | 0 | None | None major. |
| `getConfig()` | No | 0 | Returns cached config reference | Raw register writes can make cache stale. |
| `probe()` | Yes | 2 raw reads | No health tracking | I2C error class collapsed to `DEVICE_NOT_FOUND`. |
| `recover()` | Yes | 4 on success | Allows I2C while offline, reapplies config and Mask/Enable cache | Does not reapply alert/PV limits; partial failure dirty state missing. |
| `getSettings(out)` | No | 0 | None | Does not include alert/PV limit cache because none exists. |
| `state()` | No | 0 | None | None major. |
| `isOnline()` | No | 0 | Treats `READY` and `DEGRADED` as online | DEGRADED meaning should be documented for field policy. |
| `lastOkMs()` / `lastErrorMs()` | No | 0 | None | Timestamps are 0 without `nowMs` hook. |
| `lastError()` | No | 0 | None | None major. |
| `consecutiveFailures()` | No | 0 | None | Counter wraps/saturates behavior should stay documented. |
| `totalFailures()` / `totalSuccess()` | No | 0 | None | Lifetime counters wrap at max; field telemetry should account for that. |
| `readShuntRaw(ch)` | Yes | 1, or readiness check + 1 in triggered mode | Triggered path may read Mask/Enable | Raw is register word, not shifted counts. |
| `readBusRaw(ch)` | Yes | 1, or readiness check + 1 in triggered mode | Triggered path may read Mask/Enable | Same raw-word contract. |
| `readShuntVoltage(ch)` | Yes | Same as shunt raw | Same as shunt raw | None major. |
| `readBusVoltage(ch)` | Yes | Same as bus raw | Same as bus raw | 26 V physical limit should be front-facing. |
| `readCurrent(ch)` | Yes | Same as shunt raw | Uses configured shunt value | Default shunt can give plausible wrong current. |
| `readPower(ch)` | Yes | 2 reads | Bus/shunt not atomic | Atomicity/staleness not documented. |
| `readChannel(ch)` | Yes | 2 reads; maybe +1 CVRF read | Bus/shunt not atomic; triggered may clear flags | Atomicity/staleness not documented. |
| `readShuntSumRaw()` | Yes | 1 | None beyond health | Equal-shunt warning missing. |
| `readShuntSumVoltage()` | Yes | 1 | None beyond health | Sum is voltage, not total current unless equal shunts. |
| `startConversion()` | Yes | 1 write | Starts single-shot, clears CVRF per config write | Busy/retrigger contract needs more tests. |
| `startConversion(mode)` | Yes | 1 write | Same | Same. |
| `readConversionReady(ready)` | Yes | 0 or 1 | Reads Mask/Enable and clears CVRF/alerts | Side effect documented here but not across implicit callers. |
| `conversionReady()` | Yes | 0 or 1 | Same | Errors collapse to `false`. |
| `readBlocking(ch1,ch2,ch3,timeout)` | Yes | Continuous: `2*n`; triggered: `1 + polls + 2*n` | Starts conversion, polls Mask/Enable, may clear alerts | Requires monotonic `nowMs`; continuous is latest-sample, not fresh wait. |
| `setMode(mode)` | Yes | 1 write | Writes config; triggered mode starts conversion | 40 us power-down recovery not modeled. |
| `getMode()` | No | 0 | None | Cached value can be stale after raw writes. |
| `setAveraging(avg)` | Yes | 1 write | Writes config; rolls back cache on failure | Averaging timing semantics unclear. |
| `getAveraging()` | No | 0 | None | Cached value can be stale after raw writes. |
| `setVBusConvTime(ct)` | Yes | 1 write | Writes config | None major. |
| `getVBusConvTime()` / `getVbusConvTime()` | No | 0 | None | Cached value can be stale after raw writes. |
| `setVbusConvTime(ct)` | Yes | 1 write | Alias of `setVBusConvTime()` | Same contract as primary API. |
| `setVShuntConvTime(ct)` | Yes | 1 write | Writes config | None major. |
| `getVShuntConvTime()` / `getVshuntConvTime()` | No | 0 | None | Cached value can be stale after raw writes. |
| `setVshuntConvTime(ct)` | Yes | 1 write | Alias of `setVShuntConvTime()` | Same contract as primary API. |
| `setChannelEnable(ch,en)` | Yes | 1 write | Writes config; rejects disabling last channel in active modes | Power-down all-disabled contract should be explicit. |
| `getChannelEnable(ch)` | No | 0 | Returns false for invalid channel | Status-returning getter missing. |
| `setShuntResistance(ch,ohms)` | No I2C | 0 | Changes host current/power math | Calibration/default warning. |
| `getShuntResistance(ch)` | No | 0 | Returns 0.0f for invalid channel | Status-returning getter missing. |
| `readConfig(config)` | Yes | 1 read | None beyond health | None major. |
| `writeConfig(config)` | Yes | 1 write | Syncs config cache; reset bit clears writable cache | Raw writes can desync hardware/cache around limits. |
| `softReset()` | Yes | 1 write | Resets device/cache to defaults | Does not reapply limits; PV behavior needs contract. |
| Alert/PV limit setters | Yes | 1 write | Writes raw thresholds with reserved-bit masks | No cache; no mV helpers; PV topology contract missing. |
| Alert/PV limit getters | Yes | 1 read | None beyond health | Raw register format must be clearer. |
| `readAlertFlags(flags)` | Yes | 1 read | Clears CF/SF/WF/CVRF per datasheet | Needs direct tests and cached-cleared snapshot story. |
| `setSummationChannels()` | Yes | 1 write | Writes Mask/Enable cache | Equal-shunt guard/warning missing. |
| `setAlertLatchEnable()` | Yes | 1 write | Writes Mask/Enable cache | SMBus alert response requires latch mode if supported. |
| `readManufacturerId()` / `readDieId()` | Yes | 1 read each | Tracked health | None major. |
| `readRegister16()` | Yes | 1 read | Tracked health | Address validity only; read-clear effects not surfaced by register. |
| `writeRegister16()` | Yes | 1 write | Tracked health; raw writes may desync cache | Does not reject read-only registers. |
| `shuntRawToMv(raw)` | No | 0 | Pure conversion helper | Raw parameter is register word, not shifted count. |
| `busRawToVolts(raw)` | No | 0 | Pure conversion helper | Raw parameter is register word; physical bus limit is still 26 V. |
| `mvToShuntRaw(mV)` | No | 0 | Pure encode helper | Alert-limit helper ambiguity remains separate. |
| `voltsToBusRaw(volts)` | No | 0 | Pure encode helper | Should not imply safe physical input above 26 V. |
| `getConversionTimeUs()` | No | 0 | Uses cached conversion-time settings | Name returns per-enabled-signal sum, not full cycle. |
| `getCycleTimeUs()` | No | 0 | Uses cached channels/mode/averaging | Averaging multiplier is not datasheet-proven for CVRF latency. |

## Scaling and Units Assessment

- Shunt registers: signed two's-complement field in bits `[15:3]`, 40 uV/LSB. Code uses `DATA_SHIFT = 3`, `SHUNT_LSB_MV = 0.04f`, and explicit sign extension. This is correct.
- Bus registers: signed field in bits `[15:3]`, 8 mV/LSB. Code uses `BUS_LSB_V = 0.008f` and explicit sign extension. This is correct for register math. Documentation must still warn that physical bus input must not exceed 26 V even though register scale reaches 32.76 V.
- Raw APIs: `readShuntRaw()` and `readBusRaw()` return register words, not shifted ADC counts. This is defensible but under-documented.
- Current: host-side only, `mA = shunt_mV / ohms`. The API does not pretend INA3221 reports current directly. The default `0.1 ohm` shunt can produce plausible wrong values if not overridden.
- Power: host-side only, `mW = bus_V * current_mA`. Bus and shunt reads are not atomic.
- Alert critical/warning limits: raw datasheet-format setters mask reserved bits. No mV helpers are present, which is conservative because the local datasheet extract has an alert-limit example ambiguity.
- Shunt sum: raw sum uses bits `[15:1]`, 40 uV/LSB. The library does not guard equal shunt values for current summation.
- Power-valid limits: raw setters mask to `0x7FF8`; datasheet extracts show sign bit in the row but reset/data fields use bits 14-3. Negative raw inputs are accepted and masked. This should be clarified before adding voltage helpers.

## Timing Model

Datasheet ADC scan model:

```text
T_scan_us = enabled_channels *
            ((mode_has_shunt ? t_shunt_us : 0) +
             (mode_has_bus   ? t_bus_us   : 0))
```

Current code readiness model:

```text
T_code_ready_us = T_scan_us * averaging_sample_count
T_poll_gate_ms  = ceil(T_code_ready_us / 1000) + 1
```

The datasheet supports the scan formula and says averaging changes response/filter behavior. This audit did not find text proving CVRF latency equals `T_scan_us * averaging_sample_count`.

| Configuration | Enabled channels | Signals | Conversion time | Approx update behavior | Notes |
| --- | ---: | --- | --- | --- | --- |
| 1 ch, shunt only, AVG_1 | 1 | shunt | 140 us | 0.140 ms scan | Code waits same. |
| 3 ch, shunt+bus, AVG_1 | 3 | shunt+bus | 332 us each | 1.992 ms scan | Code waits same. |
| 3 ch, shunt+bus, AVG_1 | 3 | shunt+bus | 1.1 ms each | 6.600 ms scan | Code waits same. |
| 3 ch, shunt+bus, AVG_1024 | 3 | shunt+bus | 1.1 ms each | 6.600 ms scan; averaged response slower | Code waits about 6.758 s before CVRF read. |
| 3 ch, shunt+bus, AVG_1 | 3 | shunt+bus | 8.244 ms each | 49.464 ms typ scan | Code waits same. |
| 3 ch, shunt+bus, AVG_1024 | 3 | shunt+bus | 8.244 ms each | 49.464 ms scan; averaged response slower | Code waits about 50.651 s before CVRF read. |

Blocking and determinism notes:

- Core I2C transaction duration is delegated to transport callbacks via `i2cTimeoutMs`.
- `readBlocking()` uses deadline math and a poll cap, but wall-time behavior depends on a valid monotonic `Config::nowMs`.
- Without `nowMs`, `_nowMs()` returns 0; `readBlocking()` remains bounded by poll count but can busy-spin.
- Continuous mode reads latest registers and does not wait for a fresh conversion.

## Alert and Side-Effect Assessment

- Configuration writes clear CVRF per datasheet except power-down/disable-mode selections. Driver tracks triggered-mode config writes as conversion start.
- Mask/Enable reads clear CF1-CF3, SF, WF1-WF3, and CVRF. PVF and TCF reflect pin state and do not clear the same way.
- `readAlertFlags()` explicitly reads Mask/Enable and documents read-clear behavior.
- `readConversionReady()` explicitly reads Mask/Enable and documents read-clear behavior.
- `tick()`, `_ensureMeasurementReadyForRead()`, and `readBlocking()` can read Mask/Enable implicitly. This is the dangerous hidden side effect.
- `setSummationChannels()` and `setAlertLatchEnable()` write Mask/Enable from the cached writable bits only. This avoids writing latched flags back, but only SCC/WEN/CEN are cached.
- SMBus alert response is not implemented; latch enable exists but no alert-response API exists.

## Partial-State and Cache Consistency Assessment

Known multi-step or cache-sensitive paths:

- `begin()` resets runtime/cache, probes IDs, writes config, then marks initialized. If probe/config write fails, driver remains uninitialized and cache is reset.
- `recover()` reads IDs, writes config, writes Mask/Enable cache. If config succeeds and Mask/Enable write fails, hardware and cache can diverge and no dirty flag is exposed.
- `softReset()` writes `RST` and resets cached config/mask state, but does not restore alert/PV limit registers.
- `writeConfig()` with `RST` syncs config defaults and clears `_maskEnableWritableCache`; it does not account for non-config writable registers.
- Alert/PV limit setters do not cache values, so any reset/recover cannot reapply them.
- Raw `writeRegister16()` can write config/mask/limit registers behind the cache. README warns this can desync cached config, but no diagnostic flag exists.

Recommended diagnostic additions:

- `bool syncNeeded` in `SettingsSnapshot`.
- Cached copies and valid bits for all writable limit registers.
- `Status applyCachedSettings()` or `recover(RecoverMode::ReapplyAll)`.
- Public method to clear/inspect dirty state after raw writes.

## Tests and CI Assessment

Tests present:

- Native Unity suite in `test/test_basic.cpp`; 56 tests currently pass.
- Fake transport exists with register storage, read/write call counts, fake time, and fake yield.
- Tests cover basic status, config validation, begin/probe/recover health transitions, offline latch, read basics, setters, rollback for some config setters, reset cache sync, alert/PV reserved-bit masks, conversion helpers, cycle time, blocking timeout, raw transport validation, and invalid raw register addresses.

Important missing tests:

- Address coverage for all `0x40`-`0x43`.
- Sequenced ID verification and transport error class preservation.
- Negative signed conversion (`0xC180` style) for shunt and bus.
- Reserved low-bit behavior for measurement conversion.
- Full config encode/decode matrix for all modes, averaging, and conversion times.
- Invalid channel no-I2C tests across all channel-taking APIs.
- Mask/Enable read-clear side effects for `readAlertFlags()`, `readConversionReady()`, `tick()`, and triggered reads.
- Single-shot retrigger after CVRF read.
- Alert limit getter/setter round trips including negative values.
- Power-valid threshold defaults and voltage helper cases.
- Partial multi-register failures in `recover()` and `softReset()`.
- Core propagation/tracking for `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, `I2C_BUS`.
- No-timebase/no-yield behavior.
- Copy/move prevention compile-time checks.
- Expanded framework leakage guard.

Commands run and results:

```text
git status --short                                  -> clean before branch creation
git branch --show-current                           -> main, then audit/ina3221-industry-readiness-exploration
git rev-parse --show-toplevel                       -> C:/Users/HonzovoSpectre/Documents/Projects/INA3221
python --version                                    -> Python 3.13.12
python -m platformio --version                      -> PlatformIO Core 6.1.19
python tools/check_core_timing_guard.py             -> PASSED
python tools/check_cli_contract.py                  -> PASSED
python tools/check_idf_example_contract.py          -> PASSED
python scripts/generate_version.py check            -> Version.h up to date
python -m platformio test -e native                 -> PASSED, 56/56 tests
python -m platformio run -e esp32s3dev              -> SUCCESS
python -m platformio run -e esp32s2dev              -> SUCCESS
python -m platformio pkg pack                       -> SUCCESS, tarball created then removed
idf.py --version                                    -> failed, idf.py not found in this shell
```

Commands not run and why:

- `idf.py -C examples/esp_idf/basic set-target esp32s3 build`
- `idf.py -C examples/esp_idf/basic set-target esp32s2 build`

Reason: `idf.py` is not available in this environment, so native ESP-IDF builds cannot be claimed.

## ESP-IDF Port Assessment

- Is there a pure ESP-IDF component? **Yes.** Root `CMakeLists.txt` registers `src/INA3221.cpp` and `include`; `idf_component.yml` targets ESP32-S2/S3 with `idf >=6.0.0`.
- Is there a pure ESP-IDF example? **Yes.** `examples/esp_idf/basic` uses `app_main`, `driver/i2c_master.h`, `esp_timer`, FreeRTOS waits/yields, and fixed C buffers.
- Does it avoid Arduino dependencies? **Yes by static scan and guard script.** No Arduino/Wire/String/Serial/TwoWire tokens were found under the IDF example.
- Does it use injected/non-owning bus transport? **Partially.** The core is injected/non-owning. The example adapter owns a singleton bus/device for demo convenience and passes callbacks to the driver.
- Does it lock shared bus access if multitask? **No.** No mutex/semaphore/lock was found in the IDF adapter.
- Does it map IDF errors precisely? **Partially.** Timeout maps to `I2C_TIMEOUT`; invalid response/NACK maps to generic `I2C_ERROR`; other errors map to `I2C_BUS`.
- Does CI build ESP-IDF for ESP32-S2/S3? **No.** CI runs PlatformIO Arduino/native and static guards; no `idf.py` job was found.

## Hardware Validation Matrix Needed

Required real-hardware tests before production release:

| Area | Required validation |
| --- | --- |
| Addresses | All four A0 strap addresses: `0x40`, `0x41`, `0x42`, `0x43`. |
| Identity | Manufacturer `0x5449`, Die `0x3220`, wrong-device behavior. |
| Channels | All three channels with known shunt and bus voltages. |
| Shunt reads | Positive and, if physically safe, negative shunt voltage. |
| Bus reads | 0 V to safe representative rails; never exceed 26 V physical input. |
| Channel enable | Disable/enable each channel and verify scan timing/register updates. |
| Single-shot | Trigger, CVRF timing, power-down after conversion, re-trigger. |
| Continuous mode | Update rate vs conversion time/channel count/averaging. |
| Alerts | Critical alert pin, Warning alert pin, summation alert, latch modes. |
| Power-valid | PV upper/lower thresholds, VPU pull-up/level shifting, unused-channel wiring. |
| Timing-control | TC sequencing with and without early config writes, if feature remains supported. |
| SMBus alert | Alert response address and latch-mode requirement, if supported. |
| Faults | Address NACK, data NACK if injectable, timeout, bus error, stuck SCL/SDA if safe. |
| Recovery | Unplug/replug, brownout, software reset, external reset. |
| Soak | Long continuous read soak with health counters and no memory growth. |
| IDF | Native IDF build and run on ESP32-S2 and ESP32-S3. |
| Arduino | PlatformIO Arduino examples on ESP32-S2 and ESP32-S3 with real bus. |

## Implementation Roadmap

### Phase 1 - Core architecture and status contracts

Likely files:

- `include/INA3221/INA3221.h`
- `include/INA3221/Status.h`
- `src/INA3221.cpp`
- `test/test_basic.cpp`
- `tools/check_core_timing_guard.py`
- `README.md`

Work:

- Delete copy/move operations.
- Add explicit thread/ISR safety documentation.
- Add status-returning or diagnostic alternatives for lossy APIs.
- Decide whether to add `Err::OFFLINE`.
- Split readable/writable register validation.
- Expand framework leakage guard.

Risks:

- Breaking API if changing `tick()`/`end()` signatures or adding `Err` values not append-only.

Required tests:

- Copy/move static asserts.
- Invalid getter/channel tests.
- Read-only register write rejection.
- Guard script coverage.

### Phase 2 - INA3221 register/scaling correctness

Likely files:

- `include/INA3221/CommandTable.h`
- `include/INA3221/INA3221.h`
- `src/INA3221.cpp`
- `test/test_basic.cpp`
- `README.md`

Work:

- Clarify raw register word vs count APIs.
- Add negative sign-extension tests.
- Add full config encode/decode matrix.
- Decide PV sign-bit/raw mask semantics before voltage helpers.
- Add safe current/power calibration contract.

Risks:

- Changing raw API semantics would be breaking; prefer new aliases/helpers.

Required tests:

- Negative `0xC180` cases.
- Reserved-bit conversion cases.
- All modes/averaging/conversion-time encode/decode.

### Phase 3 - Conversion timing and readiness semantics

Likely files:

- `src/INA3221.cpp`
- `include/INA3221/INA3221.h`
- `test/test_basic.cpp`
- `README.md`
- `docs/`

Work:

- Split scan time from averaged response time.
- Revisit CVRF polling gate.
- Document continuous latest-sample semantics.
- Add no-timebase behavior tests.
- Build hardware CVRF timing matrix.

Risks:

- Existing tests expect AVG-scaled `getCycleTimeUs()`.

Required tests:

- Scan formula tests.
- `readBlocking()` timeout/no-timebase tests.
- Single-shot retrigger tests.

### Phase 4 - Alerts, PV, TC, and side-effect-safe APIs

Likely files:

- `include/INA3221/INA3221.h`
- `include/INA3221/Config.h`
- `src/INA3221.cpp`
- `examples/01_basic_bringup_cli/main.cpp`
- `examples/esp_idf/basic/main/main.cpp`
- `README.md`

Work:

- Add side-effect-safe alert/CVRF design.
- Add internal cleared-flag snapshot or make implicit reads opt-in.
- Add PV validated helper/topology docs.
- Decide TC supported vs diagnostic-only.
- Add SMBus alert response only if scope includes it.
- Add summation equal-shunt safe helper or warning.

Risks:

- Alert behavior changes can be semver-major if API contracts change.
- TC support may need hardware-specific sequencing.

Required tests:

- Mask/Enable read-clear fake tests.
- PV mode/topology validation tests.
- TC sequencing tests with hardware.

### Phase 5 - Tests/fault injection/guards

Likely files:

- `test/test_basic.cpp`
- `test/` new fake transport helpers
- `tools/check_core_timing_guard.py`
- `.github/workflows/ci.yml`

Work:

- Build a programmable fake I2C transport with queued transactions and read-clear hooks.
- Add per-transaction failure injection.
- Add address/timeout assertions.
- Extend CI static guards.

Risks:

- Test suite may need restructuring to stay readable.

Required tests:

- The missing test list in "Tests and CI Assessment".

### Phase 6 - ESP-IDF and Arduino examples

Likely files:

- `examples/common/I2cTransport.h`
- `examples/01_basic_bringup_cli/main.cpp`
- `examples/esp_idf/basic/main/*`
- `docs/IDF_PORT.md`
- `.github/workflows/ci.yml`
- `README.md`

Work:

- Add ESP-IDF `idf.py` CI if feasible.
- Document IDF singleton demo limitations.
- Add optional IDF bus lock or production adapter sample.
- Improve Arduino timeout documentation.
- Add safety warnings and rail/shunt naming guidance in examples.

Risks:

- IDF version in `idf_component.yml` is `>=6.0.0`; CI image availability may constrain this.

Required tests:

- `idf.py` builds for ESP32-S2/S3.
- Static IDF contract guard.
- Hardware smoke for both frameworks.

### Phase 7 - Hardware validation and release polish

Likely files:

- `README.md`
- `CHANGELOG.md`
- `library.json`
- `docs/`
- release notes/test logs

Work:

- Execute hardware matrix.
- Record logs and board wiring.
- Update README safety/contracts.
- Update changelog and version.
- Prepare release branch/tag only after validation.

Risks:

- Hardware may reveal timing/alert/PV ambiguities requiring API changes.

Required tests:

- Full hardware validation matrix above.
- Long soak.
- Fault injection where safe.

## Proposed Future Branch

```text
hardening/ina3221-industry-readiness
```

Do not start that implementation branch until explicitly requested.

## Final Verdict

The first hardening work should address side-effect-safe readiness/alert handling, reset/recover cache consistency, and the timing model. Those are the largest production blockers because they can silently lose fault evidence, misrepresent rail-good state, or add multi-second latency in high-averaging configurations.

After that, add the programmable fake transport and missing tests before broad API expansion. The core architecture is solid enough to harden, but the library should not be called industry-grade until native ESP-IDF builds and real hardware/fault validation are part of the evidence.
