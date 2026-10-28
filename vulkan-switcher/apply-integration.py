#!/usr/bin/env python3
"""Apply the vulkan-switcher integration to the avium tree (idempotent)."""
import io
import os
import sys

TREE = "/mnt/build/avium"
DEVICE_MK = f"{TREE}/device/oneplus/ovaltine/device.mk"
VENDOR_PROP = f"{TREE}/device/oneplus/sm8450-common/vendor.prop"
FILE_CONTEXTS = f"{TREE}/device/oneplus/sm8450-common/sepolicy/file_contexts"
INIT_RC = f"{TREE}/device/oneplus/sm8450-common/init/init.qcom.rc"
BOARD_MK = f"{TREE}/device/oneplus/ovaltine/BoardConfig.mk"


def read(path):
    with io.open(path, "r", encoding="utf-8") as f:
        return f.read()


def write(path, text):
    with io.open(path, "w", encoding="utf-8") as f:
        f.write(text)


def replace_block(text, start_marker, end_marker, replacement, label):
    start = text.find(start_marker)
    if start < 0:
        print(f"SKIP {label}: start marker not found")
        return text
    end = text.find(end_marker, start)
    if end < 0:
        print(f"SKIP {label}: end marker not found")
        return text
    end += len(end_marker)
    return text[:start] + replacement + text[end:]


NEW_DEVICE_BLOCK = """# Vulkan ICD switching without touching the framework.
#
# /vendor/lib64/hw/ carries vulkan.adreno.so (OEM Qualcomm) and
# vulkan.freedreno.so (Mesa Turnip). ro.hardware.vulkan no longer chooses
# between them: it points at vulkan.switcher.so, a small hwvulkan HAL module
# that adopts the real ICD named in
#   /data/misc/vulkan/per_app.conf     (writable, wins)
#   /vendor/etc/vulkan/per_app.conf    (image default, see below)
# and hands that driver's device struct to libvulkan unchanged. libvulkan itself
# is stock, so a framework update can never lose the switch.
#
#   <package>=<subname>    e.g. com.tungsten.fcl=freedreno
#   *=<subname>            default for every other process
#
# Both ICDs always ship, so any app can be moved at runtime by editing the
# /data file and force-stopping that app -- no rebuild, no reboot, no root.
#
# GPU_DRIVER_TURNIP now only picks the *image default*:
#   true  (default) -> *=freedreno, the open-source ICD
#   false           -> *=adreno, the OEM ICD
GPU_DRIVER_TURNIP ?= true

PRODUCT_PACKAGES += \\
    vulkan_switcher

PRODUCT_VENDOR_PROPERTIES += ro.hardware.vulkan=switcher

ifeq ($(GPU_DRIVER_TURNIP),true)
PRODUCT_PACKAGES += vulkan.freedreno
PRODUCT_COPY_FILES += \\
    device/oneplus/ovaltine/vulkan-switcher/per_app.conf.turnip:$(TARGET_COPY_OUT_VENDOR)/etc/vulkan/per_app.conf
else
PRODUCT_COPY_FILES += \\
    device/oneplus/ovaltine/vulkan-switcher/per_app.conf.oem:$(TARGET_COPY_OUT_VENDOR)/etc/vulkan/per_app.conf
endif

# The privileged selector app that used to rewrite /data/misc/vulkan/per_app.conf
# was not carried over into this tree; naming it here would fail the build with
# an unknown module. Until it lands, use /vendor/bin/vulkan-driver (root shell)
# or edit the file directly.
# PRODUCT_PACKAGES += VulkanDriverSwitch
"""


def main():
    text = read(DEVICE_MK)
    if "ro.hardware.vulkan=switcher" in text:
        print("SKIP device.mk: already integrated")
    else:
        text2 = replace_block(
            text,
            "# Mesa Turnip Vulkan driver (see BoardConfig GPU_DRIVER_TURNIP).",
            "PRODUCT_PACKAGES += VulkanDriverSwitch",
            NEW_DEVICE_BLOCK.rstrip("\n"),
            "device.mk",
        )
        if text2 == text:
            sys.exit("device.mk: nothing replaced")
        write(DEVICE_MK, text2)
        print("OK device.mk")

    # vendor.prop must not carry a second ro.hardware.vulkan assignment: the
    # loader would see whichever wins the property merge, and post_process_props
    # flags the duplicate.
    text = read(VENDOR_PROP)
    if "\nro.hardware.vulkan=adreno\n" in text:
        text = text.replace("\nro.hardware.vulkan=adreno\n",
                            "\n# ro.hardware.vulkan is set by device/oneplus/ovaltine/device.mk\n"
                            "# (vulkan.switcher), not here.\n", 1)
        write(VENDOR_PROP, text)
        print("OK vendor.prop")
    else:
        print("SKIP vendor.prop: no duplicate assignment")

    text = read(FILE_CONTEXTS)
    if "vulkan.switcher.so" in text:
        print("SKIP file_contexts: already present")
    else:
        text = text.rstrip("\n") + "\n"
        text += ("# The ICD switcher and its image-default config.\n"
                 "/vendor/lib(64)?/hw/vulkan\\.switcher\\.so    u:object_r:same_process_hal_file:s0\n"
                 "/vendor/etc/vulkan(/.*)?    u:object_r:vulkan_perapp_conf_file:s0\n")
        write(FILE_CONTEXTS, text)
        print("OK file_contexts")

    text = read(INIT_RC)
    if "/data/misc/vulkan" in text:
        print("SKIP init.qcom.rc: already present")
    else:
        anchor = "    mkdir /data/vendor/misc 01771 system system\n"
        if anchor not in text:
            sys.exit("init.qcom.rc: anchor not found")
        text = text.replace(
            anchor,
            anchor + "\n"
            "    # Writable per-app Vulkan ICD mapping (see vulkan.switcher.so).\n"
            "    mkdir /data/misc/vulkan 0770 system system\n",
            1,
        )
        write(INIT_RC, text)
        print("OK init.qcom.rc")

    text = read(BOARD_MK)
    if "per-application override (VulkanDriverSwitch) keeps working" in text:
        text = text.replace(
            "# `brunch ovaltine` installs. The OEM vulkan.adreno.so stays on disk, so\n"
            "# per-application override (VulkanDriverSwitch) keeps working and the global\n"
            "# fallback is:\n",
            "# `brunch ovaltine` installs. Both ICDs stay on disk and a runtime\n"
            "# switcher (vulkan.switcher.so, see device.mk) picks per app, so this\n"
            "# variable only chooses the image default:\n",
        )
        write(BOARD_MK, text)
        print("OK BoardConfig.mk comment")


if __name__ == "__main__":
    main()
