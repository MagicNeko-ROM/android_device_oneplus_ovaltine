#   （原行内容见 port-t5/evidence/device_oneplus_ovaltine_device.mk.before）
# Copyright (C) 2021-2025 The LineageOS Project
#   （原行内容见 port-t5/evidence/device_oneplus_ovaltine_device.mk.before）
# SPDX-License-Identifier: Apache-2.0
#   （原行内容见 port-t5/evidence/device_oneplus_ovaltine_device.mk.before）

# AAPT
PRODUCT_AAPT_CONFIG := normal
PRODUCT_AAPT_PREF_CONFIG := xxhdpi

# Alert slider
PRODUCT_PACKAGES += \
    KeyHandler \
    tri-state-key-calibrate

# Audio
# 10T(CPH2415) 的 audio XML 取自 10T 自身（旧 ovaltine 树），
# 文件名与 10 Pro(wly) 不同：ODM 侧 mixer_paths.xml / resourcemanager.xml，
# 且 10T 树没有 sound_trigger_*.xml（不要从 wly 抄）。
QCV_FAMILY_SKUS := taro cape

PRODUCT_COPY_FILES += \
$(foreach DEVICE_SKU, $(QCV_FAMILY_SKUS), \
    $(LOCAL_PATH)/audio/mixer_paths.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio/sku_$(DEVICE_SKU)/mixer_paths_waipio_mtp.xml \
    $(LOCAL_PATH)/audio/resourcemanager.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio/sku_$(DEVICE_SKU)/resourcemanager_waipio_mtp.xml)

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/audio/backend_conf.xml:$(TARGET_COPY_OUT_ODM)/etc/backend_conf.xml \
    $(LOCAL_PATH)/audio/mixer_paths.xml:$(TARGET_COPY_OUT_ODM)/etc/mixer_paths.xml \
    $(LOCAL_PATH)/audio/resourcemanager.xml:$(TARGET_COPY_OUT_ODM)/etc/resourcemanager.xml \
    $(LOCAL_PATH)/audio/usecaseKvManager.xml:$(TARGET_COPY_OUT_ODM)/etc/usecaseKvManager.xml \
    $(LOCAL_PATH)/audio/usecaseKvManager.xml:$(TARGET_COPY_OUT_VENDOR)/etc/usecaseKvManager.xml

# Boot animation
TARGET_SCREEN_HEIGHT := 2412
TARGET_SCREEN_WIDTH := 1080

# Display
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/display_id_4630947180710147970.xml:$(TARGET_COPY_OUT_VENDOR)/etc/displayconfig/display_id_4630947180710147969.xml

# oplus_udfps 在 qtidisplay 命名空间里声明为 bool_variables → 必须用 soong_config_set_bool
# （wly 树用的是 soong_config_set，属字符串写法，与 bool 声明不匹配，不要照抄）
$(call soong_config_set_bool,qtidisplay,oplus_udfps,true)
# 10T(ovaltine) 无 Pixelworks iris 硬件，不设 pxlw_vendor_namespace；
# 否则 hardware/qcom-caf/sm8550/display 的 soong_config select 会要求 libpwirisfeature。
#   （原行内容见 port-t5/evidence/device_oneplus_ovaltine_device.mk.before）

# Fingerprint
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.fingerprint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.fingerprint.xml

$(call soong_config_set,surfaceflinger,udfps_lib,//hardware/oplus:libudfps_extension.oplus)

# AIDL fingerprint shim：用本树（A16 头文件）编的版本替换原厂 ODM 里那份（A15 契约）。
# 原厂 shim 在 A16 上于 SensorProps 拷贝构造抛 length_error -> SIGABRT，
# 导致 getSensorProps() 失败 -> BiometricServiceRegistry: No sensors found。
# device_specific:true => 装到 /odm/lib64/；源码模块优先于同名 prebuilt。
PRODUCT_PACKAGES += \
    libshims_aidl_fingerprint_v2.oplus

# NFC
PRODUCT_PACKAGES += \
    android.hardware.nfc-service.nxp \
    com.android.nfc_extras \
    Tag

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.nfc.ese.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.ese.xml \
    frameworks/native/data/etc/android.hardware.nfc.hce.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.hce.xml \
    frameworks/native/data/etc/android.hardware.nfc.hcef.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.hcef.xml \
    frameworks/native/data/etc/android.hardware.nfc.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.xml \
    frameworks/native/data/etc/android.hardware.se.omapi.ese.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.se.omapi.ese.xml \
    frameworks/native/data/etc/android.hardware.se.omapi.uicc.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.se.omapi.uicc.xml \
    frameworks/native/data/etc/com.android.nfc_extras.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/com.android.nfc_extras.xml \
    frameworks/native/data/etc/com.nxp.mifare.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/com.nxp.mifare.xml

# Overlays
DEVICE_PACKAGE_OVERLAYS += \
    $(LOCAL_PATH)/overlay-lineage

PRODUCT_PACKAGES += \
    OPlusFrameworksResTarget \
    OPlusSettingsProviderResTarget \
    OPlusSettingsResTarget \
    OPlusSystemUIResTarget

# Power
$(call soong_config_set,qtipower,mode_ext_lib,power-ext-oplus)

# PowerShare
PRODUCT_PACKAGES += \
    vendor.lineage.powershare-service.oplus

# Soong namespaces
PRODUCT_SOONG_NAMESPACES += \
    $(LOCAL_PATH)

# Inherit from the common OEM chipset makefile.
$(call inherit-product, device/oneplus/sm8450-common/common.mk)

# Inherit from the proprietary files makefile.
$(call inherit-product, vendor/oneplus/ovaltine/ovaltine-vendor.mk)
