#
# Copyright (C) 2021-2025 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Include the common OEM chipset BoardConfig.
include device/oneplus/sm8450-common/BoardConfigCommon.mk

DEVICE_PATH := device/oneplus/ovaltine

# HIDL
# 10T 有 eSE/NFC 与 qti esepowermanager 两个 HIDL HAL；
# sm8450-common 的 vintf/manifest.xml 未声明它们，必须由机型补。
DEVICE_MANIFEST_FILE += $(DEVICE_PATH)/manifest.xml

# Partitions
# 10T 的 super 分区布局沿用 stock：super 总大小与 common 默认一致（11274289152），
# 只在 product 上预留 900000000 B。BOARD_ONEPLUS_DYNAMIC_PARTITIONS_SIZE 是
# 旧树遗留的死变量（全树无引用），本次不继承。
ifeq (,$(filter true, $(WITHOUT_RESERVED_SIZE) $(WITH_GMS)))
BOARD_PRODUCTIMAGE_PARTITION_RESERVED_SIZE := 900000000
endif

# Properties
TARGET_VENDOR_PROP += $(DEVICE_PATH)/vendor.prop

# Recovery
# 10T: 1080x2412 → xxhdpi；状态栏/挖孔高度与 10 Pro 不同（103 vs 126）。
TARGET_RECOVERY_DENSITY := xxhdpi
TARGET_RECOVERY_UI_MARGIN_HEIGHT := 103

# Include the proprietary files BoardConfig.
include vendor/oneplus/ovaltine/BoardConfigVendor.mk

# vendor/extra board config
-include vendor/extra/BoardConfigExtra.mk

# Third-party Adreno driver selection (Meta Quest 3 V@0837.0.7; the files live
# in vendor/oneplus/sm8450-common/gpu-q3, wired up by that directory's own
# gpu-q3.mk).
# ON BY DEFAULT since 2026-10-22, so a plain `brunch ovaltine` ships the Q3
# stack. `?=` only assigns when unset, so the environment override still wins
# and falling back to the OEM driver is:
#   GPU_DRIVER_Q3=false brunch ovaltine
GPU_DRIVER_Q3 ?= true
$(call soong_config_set,gpu_oplus,driver_q3,$(GPU_DRIVER_Q3))

# The original fingerprint HAL is renamed to IFingerprint/oplusfp so the proxy
# HAL can register IFingerprint/default. android.* HALs using a non-standard
# instance name must be listed here or checkvintf fails the build.
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE += \
    device/oneplus/ovaltine/fingerprint/compatibility_matrix.xml

# Mesa Turnip (freedreno) Vulkan driver switch. Independent from
# GPU_DRIVER_Q3: Q3 only swaps the Adreno stack, Turnip replaces the Vulkan
# driver itself with the open-source one.
# ON BY DEFAULT since 2026-10-22: the open-source Vulkan ICD is what a plain
# `brunch ovaltine` installs. Both ICDs stay on disk and a runtime
# switcher (vulkan.switcher.so, see device.mk) picks per app, so this
# variable only chooses the image default:
#   GPU_DRIVER_TURNIP=false brunch ovaltine
GPU_DRIVER_TURNIP ?= true
$(call soong_config_set_bool,gpu_oplus,driver_turnip,$(GPU_DRIVER_TURNIP))
