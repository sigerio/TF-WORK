from pathlib import Path
import re
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
MDK = ROOT / "MDK"
project = ET.parse(MDK / "wether.uvprojx")
options = ET.parse(MDK / "wether.uvoptx")
ET.parse(MDK / "wether.uvguix.1")

for path in (MDK / "wether.uvprojx", MDK / "wether.uvoptx", MDK / "wether.uvguix.1"):
    text = path.read_text(errors="ignore").replace("\\", "/")
    assert not re.search(r"(?:^|/)(?:USER|Core|FreeRTOS-Kernel|FreeRTOS-Helpful)(?:/|$)", text), path


def resolve_project_path(value):
    return (MDK / value.replace("\\", "/")).resolve()


project_files = set()
for entry in project.findall(".//Groups/Group/Files/File"):
    path = resolve_project_path(entry.findtext("FilePath"))
    assert path.is_file(), f"Missing project file: {path}"
    assert path.name == entry.findtext("FileName"), f"Wrong display name: {path}"
    assert path not in project_files, f"Duplicate project entry: {path}"
    project_files.add(path)

for entry in project.findall(".//IncludePath"):
    for value in (entry.text or "").split(";"):
        if value:
            assert resolve_project_path(value).is_dir(), f"Missing include directory: {value}"

option_files = {
    resolve_project_path(entry.text)
    for entry in options.findall(".//Group/File/PathWithFileName")
}
assert option_files == project_files, "Project and debugger file lists differ"

for path in project_files:
    relative = path.relative_to(ROOT)
    assert relative.parts[0] not in {"USER", "Core", "FreeRTOS-Kernel", "FreeRTOS-Helpful", "Reference"}, relative

kernel = ROOT / "ThirdParty/FreeRTOS/Kernel"
assert (kernel / "tasks.c").is_file()
assert (kernel / "portable/RVDS/ARM_CM3/port.c").is_file()
assert (ROOT / "ThirdParty/FreeRTOS/LICENSE.md").is_file()
assert (ROOT / "ThirdParty/FreeRTOS/History.txt").is_file()
assert (ROOT / "Application/Runtime/FreeRTOSConfig.h").is_file()
assert (ROOT / "Application/Runtime/freertos_hooks.c").is_file()
assert "task_scheduler();" in (ROOT / "Application/Startup/main.c").read_text()
scheduler = (ROOT / "Application/Runtime/task_scheduler.c").read_text()
assert "xTaskCreate(start_task" in scheduler
assert "vTaskStartScheduler();" in scheduler
assert "vTaskDelete(NULL);" in scheduler
assert "xTaskCreate(communication_rx_task" in scheduler
assert "xTaskCreate(queue_pack_read" in scheduler
hooks = (ROOT / "Application/Runtime/freertos_hooks.c").read_text()
assert "vApplicationDaemonTaskStartupHook" in hooks
assert "xTaskCreate(" not in hooks
assert "xQueueCreate(" in hooks
assert "board_init();" in hooks
assert "configUSE_DAEMON_TASK_STARTUP_HOOK    1" in (ROOT / "Application/Runtime/FreeRTOSConfig.h").read_text()
for directory in (ROOT / "Libraries/RingBuffer", ROOT / "Libraries/Modbus"):
    for path in directory.iterdir():
        if path.suffix in {".c", ".h"}:
            includes = re.findall(r'#include\s+"([^"]+)"', path.read_text())
            assert all((directory / name).is_file() for name in includes), f"Coupled library: {path}"

assert not (ROOT / "Platform/Config").exists()
assert (ROOT / "ThirdParty/GD32F10x/StandardPeripheralLibrary/Include/gd32f10x_libopt.h").is_file()
transport = (ROOT / "Platform/Communication/uart_transport.c").read_text()
assert not re.search(r'#include\s+"(?:comm_|main_task|soft_idle|myqueue)', transport)

print(f"PASS: {len(project_files)} project files, include paths and debugger entries")
print("PASS: FreeRTOS build copy and metadata present")
print("PASS: portable libraries and UART transport dependency boundaries")
