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
