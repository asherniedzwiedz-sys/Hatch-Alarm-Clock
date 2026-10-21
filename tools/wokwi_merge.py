"""PlatformIO post-build script for `pio run -e wokwi`: merges the bootloader, partition table,
app and LittleFS web page into one 4 MB flash image, firmware.merged.bin, which wokwi.toml loads.
The simulator needs a full image because this build uses a custom partition table."""
import csv
import os
import subprocess

Import("env")  # noqa: F821  (provided by PlatformIO)


def fs_offset(table):
    with open(table, newline="") as f:
        for row in csv.reader(line for line in f if line.strip() and not line.lstrip().startswith("#")):
            if len(row) > 3 and row[2].strip() in ("spiffs", "littlefs"):
                return row[3].strip()
    raise RuntimeError("no filesystem partition in " + table)


def merge(source, target, env):
    build = env.subst("$BUILD_DIR")
    images = []
    for offset, path in env.get("FLASH_EXTRA_IMAGES", []):  # bootloader, partitions, boot_app0
        images += [env.subst(offset), env.subst(path)]
    images += [env.subst("$ESP32_APP_OFFSET"), os.path.join(build, "firmware.bin")]
    fs = os.path.join(build, "littlefs.bin")
    if os.path.exists(fs):
        images += [fs_offset(env.subst("$PARTITIONS_TABLE_CSV")), fs]
    else:
        print("wokwi_merge: no littlefs.bin yet, so no web page. Run `pio run -e wokwi -t buildfs` first.")
    esptool = os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py")
    out = os.path.join(build, "firmware.merged.bin")
    subprocess.check_call([env.subst("$PYTHONEXE"), esptool, "--chip", "esp32", "merge_bin", "-o", out,
                           "--flash_mode", "keep", "--flash_freq", "keep", "--flash_size", "4MB"] + images)
    print("wokwi_merge: wrote " + out)


env.AddPostAction("$BUILD_DIR/firmware.bin", merge)  # noqa: F821
