"""Run host Modbus, motor and link checks plus ARM configuration checks."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(command, *, expected_error=None):
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    if expected_error:
        assert result.returncode != 0 and expected_error in result.stderr, result.stdout + result.stderr
    else:
        assert result.returncode == 0, result.stdout + result.stderr


with tempfile.TemporaryDirectory(prefix="xj_app_safety_") as directory:
    binary = str(Path(directory) / "test_app_safety")
    run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-DGD32F10X_HD",
         "-ICMSIS", "-ICore", "-IGD32F10X/Include", "-IUSER", "tests/test_app_safety.c", "USER/APP/app_status.c",
         "USER/APP/tasks/app_timer_task.c", "USER/CONTROL/motor/motor_control.c", "-o", binary])
    for boot_pressed in ("0", "1"):
        run([binary, boot_pressed])
    motor_binary = str(Path(directory) / "test_motor_limits")
    # Vendor Ethernet headers declare static functions; hardware stubs intentionally ignore unused parameters.
    host = ["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-Wno-unused-function",
            "-DGD32F10X_HD", "-ICMSIS", "-ICore", "-IGD32F10X/Include", "-IUSER"]
    link_binary = str(Path(directory) / "test_link_tx")
    run(host + ["-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
        "tests/test_link_tx.c", "USER/BOARD/board.c", "-o", link_binary])
    run([link_binary])
    ws2812_binary = str(Path(directory) / "test_ws2812_debug")
    run(host + ["-Wno-pointer-to-int-cast", "-Wno-sign-compare", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                "tests/test_ws2812_debug.c", "USER/CONTROL/ws2812/ws2812_status.c", "USER/CONTROL/ws2812/ws2812_effect.c",
                "USER/COMMON/ringbuf.c", "-o", ws2812_binary])
    run([ws2812_binary])
    encoding_binary = str(Path(directory) / "test_ws2812_encoding")
    run(host + ["-Wno-pointer-to-int-cast", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                "tests/test_ws2812_encoding.c", "USER/CONTROL/ws2812/ws2812_driver.c", "-o", encoding_binary])
    run([encoding_binary])
    modbus_sources = ["tests/test_modbus.c", "USER/COMM/modbus/modbus_master.c", "USER/COMM/modbus/modbus_devices.c",
                      "USER/COMM/modbus/modbus_txq.c", "USER/COMM/modbus/modbus_rtu.c", "USER/COMMON/ringbuf.c",
                      "USER/COMMON/circular_index.c", "USER/APP/app_status.c"]
    modbus_sources += sorted(str(path.relative_to(ROOT)) for path in (ROOT / "USER/COMM/modbus/devices").glob("*.c"))
    for diagnostics in (0, 1):
        modbus_binary = str(Path(directory) / "test_modbus")
        run(host + ["-DBOARD_USART2_ROLE=1", f"-DMODULE_MODBUS_ENABLE_DIAG={diagnostics}", "-ffunction-sections",
                    "-fdata-sections", "-Wl,--gc-sections"] + modbus_sources + ["-o", modbus_binary])
        run([modbus_binary])
    motor_sources = ["tests/test_motor_limits.c", "USER/BOARD/board.c", "USER/CONTROL/motor/motor_driver.c",
                     "USER/CONTROL/motor/motor_control.c", "USER/CONTROL/output/aux_output.c", "USER/APP/app_status.c", "USER/COMM/modbus/devices/modbus_xm8537.c",
                     "USER/COMM/modbus/devices/modbus_common.c", "USER/COMM/modbus/modbus_rtu.c"]
    report_sources = ["USER/COMM/iec104/app/iec104_app.c", "USER/COMM/iec104/app/iec104_dispatch.c", "USER/COMM/iec104/core/iec104_asdu.c"]
    run(host + ["-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections"] + motor_sources + report_sources + ["-o", motor_binary])
    run([motor_binary])
    for combination in (3, 7, 11, 12, 13, 14, 15):
        for phase in ("boot", "idle", "moving", "pending", "reversing", "command"):
            run([motor_binary, str(combination), phase])

arm = ["arm-none-eabi-gcc", "-fsyntax-only", "-std=c99", "-mcpu=cortex-m3", "-mthumb",
       "-DGD32F10X_HD", "-ICMSIS", "-ICore", "-IGD32F10X/Include", "-IUSER"]
sources = sorted(str(path.relative_to(ROOT)) for path in (ROOT / "USER").rglob("*.c"))
sources += ["Core/main.c", "Core/gd32f10x_it.c"]
for role in (0, 1, 2):
    run(arm + [f"-DBOARD_USART2_ROLE={role}"] + sources)
for capacity in (1, 2, 4, 8, 16, 32, 64, 128):
    run(arm + [f"-DOS_POOL_MAX={capacity}", "USER/OS/os_pool.c"])
for capacity in (-1, 0, 3, 12, 129, 256):
    run(arm + [f"-DOS_POOL_MAX={capacity}", "USER/OS/os_pool.c"], expected_error="OS_POOL_MAX must be a power of two")
with tempfile.TemporaryDirectory(prefix="xj_debug_link_") as directory:
    source = Path(directory) / "debug_link.c"
    source.write_text('#include "DEBUG/debug_console.h"\nint main(void) {\n'
                      'debug_console_init(); debug_console_irq(); debug_console_tx_dma_irq();\n'
                      'debug_console_task(); return debug_printf("disabled");\n}\n')
    for role in (1, 2):
        run([option for option in arm if option != "-fsyntax-only"] + [f"-DBOARD_USART2_ROLE={role}",
            "-nostdlib", "-Wl,-e,main", str(source), "USER/DEBUG/debug_console.c", "-o", str(Path(directory) / "debug.elf")])
print("PASS: WS2812 effect library/debug/BRG encoding, Modbus layers/timing, RS485 TX setup, optical limits and motor stop/status, estop, key/LED tasks, USART2 roles, disabled-debug linking and pool boundaries")
