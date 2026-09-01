#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

IDF_EXAMPLE_MACRO = "INA3221_EXAMPLE_PLATFORM_IDF"
CLI_SOURCE_INCLUDE = '#include "examples/01_basic_bringup_cli/main.cpp"'
REQUIRED_COMPONENTS = [
    "INA3221",
    "esp_driver_i2c",
    "esp_driver_gpio",
    "esp_timer",
    "freertos",
]
FORBIDDEN_COMPONENTS = ["esp_rom", "log", "vfs"]
REQUIRED_FILES = [
    "CMakeLists.txt",
    "idf_component.yml",
    "examples/esp_idf/basic/CMakeLists.txt",
    "examples/esp_idf/basic/components/INA3221/CMakeLists.txt",
    "examples/esp_idf/basic/main/CMakeLists.txt",
    "examples/esp_idf/basic/main/main.cpp",
    "examples/esp_idf/basic/main/Ina3221IdfI2cTransport.h",
    "examples/esp_idf/basic/main/Ina3221IdfI2cTransport.cpp",
]
MANDATORY_COMMANDS = [
    "help",
    "version",
    "scan",
    "scanina",
    "read",
    "ch",
    "shunt",
    "bus",
    "current",
    "power",
    "sum",
    "shuntraw",
    "busraw",
    "sumraw",
    "ids",
    "timing",
    "start",
    "poll",
    "cancel",
    "job",
    "mode",
    "avg",
    "vbusct",
    "vshct",
    "chen",
    "rshunt",
    "direction",
    "profile",
    "addr",
    "init",
    "end",
    "freq",
    "config",
    "reset",
    "reg",
    "wreg",
    "alerts",
    "alertsnap",
    "mask",
    "crit",
    "warn",
    "sumlim",
    "pvhi",
    "pvlo",
    "sumch",
    "latch",
    "drv",
    "diag",
    "verify",
    "mismatch",
    "probe",
    "recover",
    "online",
    "cfg",
    "verbose",
    "stress",
    "stress_mix",
    "stress_owner",
    "stress_freq",
    "hilrun",
    "hilmark",
    "xfer_reset",
    "xfer_stats",
    "xfer_assert",
    "selftest",
    "convert",
]
FORBIDDEN_IDF_TOKENS = [
    "Arduino.h",
    "Wire.h",
    "TwoWire",
    "String",
    "Serial",
    "ArduinoCompat",
    "IdfArduinoCompat",
    CLI_SOURCE_INCLUDE,
]
NATIVE_IDF_TOKENS = [
    'extern "C" void app_main(void)',
    '#include "driver/i2c_master.h"',
    "esp_timer_get_time",
    "vTaskDelay",
    "read(STDIN_FILENO",
    "char line[MAX_LINE_LEN]",
    "lineOverflow",
    "ina3221IdfProbeAddress",
    "ina3221IdfI2cWriteReadAt",
]


def fail(msg: str) -> None:
    print(f"IDF example contract FAILED: {msg}")
    raise SystemExit(1)


def require_token(text: str, token: str, label: str) -> None:
    if token not in text:
        fail(f"{label} missing token '{token}'")


def command_has_dispatch(text: str, command: str) -> bool:
    patterns = [
        rf'cmd\s*==\s*"{re.escape(command)}"',
        rf'std::strcmp\(cmd,\s*"{re.escape(command)}"\)\s*==\s*0',
        rf'argAfter\(cmd,\s*"{re.escape(command)}\s',
        rf'argAfter\(cmd,\s*"{re.escape(command)}"\)',
        rf'cmd\.startsWith\("{re.escape(command)}\s',
        rf'cmd\.startsWith\("{re.escape(command)}"\)',
    ]
    return any(re.search(pattern, text) for pattern in patterns)


def main() -> int:
    for rel in REQUIRED_FILES:
        if not (ROOT / rel).exists():
            fail(f"missing {rel}")

    if (ROOT / "examples" / "common" / "IdfArduinoCompat.h").exists():
        fail("examples/common/IdfArduinoCompat.h must not exist")

    idf_main = (ROOT / "examples" / "esp_idf" / "basic" / "main" / "main.cpp").read_text(
        encoding="utf-8", errors="replace"
    )
    if IDF_EXAMPLE_MACRO in idf_main:
        fail(f"ESP-IDF main must not define/use {IDF_EXAMPLE_MACRO}")
    for token in FORBIDDEN_IDF_TOKENS:
        if token in idf_main:
            fail(f"ESP-IDF main uses forbidden token '{token}'")
    for token in NATIVE_IDF_TOKENS:
        require_token(idf_main, token, "ESP-IDF main")
    for token in (
        "profile.mode = INA3221::Mode::SHUNT_BUS_TRIG;",
        "startTriggeredSample(",
    ):
        require_token(idf_main, token, "ESP-IDF owner-safe triggered example")
    if "setup();" in idf_main or "loop();" in idf_main:
        fail("ESP-IDF main must not call setup()/loop()")

    cmake = (
        ROOT / "examples" / "esp_idf" / "basic" / "main" / "CMakeLists.txt"
    ).read_text(encoding="utf-8", errors="replace")
    for component in REQUIRED_COMPONENTS:
        if re.search(rf"\b{re.escape(component)}\b", cmake) is None:
            fail(f"ESP-IDF CMake missing required component '{component}'")
    for component in FORBIDDEN_COMPONENTS:
        if re.search(rf"\b{re.escape(component)}\b", cmake) is not None:
            fail(f"ESP-IDF CMake should not require stale component '{component}'")

    project_cmake = (
        ROOT / "examples" / "esp_idf" / "basic" / "CMakeLists.txt"
    ).read_text(encoding="utf-8", errors="replace")
    if "EXTRA_COMPONENT_DIRS" in project_cmake:
        fail("ESP-IDF example must not derive the component name from the checkout directory")
    shim_cmake = (
        ROOT / "examples" / "esp_idf" / "basic" / "components" /
        "INA3221" / "CMakeLists.txt"
    ).read_text(encoding="utf-8", errors="replace")
    require_token(
        shim_cmake,
        'set(INA3221_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")',
        "fixed-name INA3221 component shim root depth",
    )
    for token in ("src/INA3221.cpp", "include"):
        require_token(shim_cmake, token, "fixed-name INA3221 component shim")

    workflow = (ROOT / ".github" / "workflows" / "ci.yml").read_text(
        encoding="utf-8", errors="replace"
    )
    if '$PWD:/INA3221' in workflow or "-w /INA3221" in workflow:
        fail("ESP-IDF CI must exercise a checkout path not named INA3221")
    if (workflow.count('$PWD:/component-source') != 4 or
            workflow.count("-w /component-source") != 4):
        fail("Every ESP-IDF CI container must use the renamed checkout path")

    transport = (
        (ROOT / "examples" / "esp_idf" / "basic" / "main" / "Ina3221IdfI2cTransport.cpp")
        .read_text(encoding="utf-8", errors="replace")
        + (ROOT / "examples" / "esp_idf" / "basic" / "main" / "Ina3221IdfI2cTransport.h")
        .read_text(encoding="utf-8", errors="replace")
    )
    for token in (
        "driver/i2c_master.h",
        "i2c_new_master_bus",
        "i2c_master_probe",
        "i2c_master_transmit",
        "i2c_master_transmit_receive",
    ):
        require_token(transport, token, "ESP-IDF transport")
    for token in FORBIDDEN_IDF_TOKENS:
        if token in transport:
            fail(f"ESP-IDF transport uses forbidden token '{token}'")

    cli = (ROOT / "examples" / "01_basic_bringup_cli" / "main.cpp").read_text(
        encoding="utf-8", errors="replace"
    )
    if f"defined({IDF_EXAMPLE_MACRO})" in cli:
        fail("Arduino CLI source must not have an IDF compatibility branch")
    for token in (
        "profile.mode = INA3221::Mode::SHUNT_BUS_TRIG;",
        "startTriggeredSample(",
    ):
        require_token(cli, token, "Arduino owner-safe triggered example")
    if CLI_SOURCE_INCLUDE in idf_main:
        fail("ESP-IDF main must not include Arduino CLI source")
    for source_name, source in (("Arduino", cli), ("native IDF", idf_main)):
        for stale_label in (" TC=%d", "TimingCtl"):
            if stale_label in source:
                fail(f"{source_name} CLI uses stale timing-control label '{stale_label}'")
        for label in ("TCF=%d", "TC_FAULT=%d", "TimingControl=%d",
                      "TimingControlFault=%d",
                      "TC_FAULT is the inverted TCF level"):
            if label not in source:
                fail(f"{source_name} CLI timing-control label '{label}' is missing")
    for command in MANDATORY_COMMANDS:
        if f'printHelpItem("{command}' not in cli:
            fail(f"Arduino CLI missing help item '{command}'")
        if not command_has_dispatch(cli, command):
            fail(f"Arduino CLI missing dispatch '{command}'")
        if f'printHelpItem("{command}' not in idf_main:
            fail(f"native IDF CLI missing help item '{command}'")
        if not command_has_dispatch(idf_main, command):
            fail(f"native IDF CLI missing dispatch '{command}'")

    help_pattern = re.compile(r'(?:cli::)?printHelpItem\("([^" ]+)')
    arduino_help = set(help_pattern.findall(cli))
    idf_help = set(help_pattern.findall(idf_main))
    if arduino_help != idf_help:
        fail(
            "Arduino/IDF normalized help command mismatch: "
            f"Arduino-only={sorted(arduino_help - idf_help)}, "
            f"IDF-only={sorted(idf_help - arduino_help)}"
        )

    for variant in (
        "job init", "job apply", "job reconcile", "job sample", "job continuous",
        "job powerdown", "job cancel", "job auto", "job step", "job result",
        "job lastsample", "job alerts",
    ):
        if variant not in cli or variant not in idf_main:
            fail(f"owner command variant '{variant}' missing from one CLI")

    for token in (
        "Last error detail:",
        "Last error msg:",
        "Managed Register Verification Evidence",
        "HIL_BEGIN token=",
        "XFER_ASSERT PASS",
        "ina3221IdfSetFrequency",
        "ina3221IdfSetAddress",
    ):
        if token not in idf_main and token not in transport:
            fail(f"native IDF expanded diagnostic/automation token '{token}' missing")

    manifest = (ROOT / "idf_component.yml").read_text(encoding="utf-8", errors="replace")
    for token in ("esp32s2", "esp32s3", "idf:"):
        require_token(manifest, token, "idf_component.yml")

    print("IDF example contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
