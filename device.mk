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
PRODUCT_PACKAGES += \
    android.hardware.biometrics.fingerprint@2.3-service.oplus

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.fingerprint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.fingerprint.xml

$(call soong_config_set,surfaceflinger,udfps_lib,//hardware/oplus:libudfps_extension.oplus)

# ---- crDroid 专属参数 -------------------------------------------------------
# TARGET_HAS_UDFPS
#   消费点：vendor/addons/config.mk:22 `ifeq ($(TARGET_HAS_UDFPS),true)`
#           → PRODUCT_PACKAGES += UdfpsIcons UdfpsAnimations
#   这是 crDroid 专属开关：LineageOS 原版没有 vendor/addons，源码里也不消费它，
#   不设时默认「关」，屏下指纹能正常解锁，只是没有 crDroid 那套图标/动画皮肤。
#   本机依据：10T(ovaltine) 为光学屏下指纹，overlay/OPlusFrameworksResTarget 已按
#           persist.vendor.fingerprint.optical.sensorlocation=540::2182 配好
#           config_udfps_sensor_props，SystemUI 压暗层与亮度表也已手调。
#   写法对齐上游 crdroidandroid/android_device_oneplus_salami device.mk:33。
TARGET_HAS_UDFPS := true
# -----------------------------------------------------------------------------

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

# Third-party Adreno graphics stack (Meta Quest 3 V@0837.0.7).
# ON BY DEFAULT since 2026-10-22. BoardConfig.mk owns the same variable and the
# two must agree; passing GPU_DRIVER_Q3=false keeps the OEM graphics modules
# instead. The OEM blobs stay untouched either way, so falling back needs no
# git operations.
GPU_DRIVER_Q3 ?= true
ifeq ($(GPU_DRIVER_Q3),true)
GPU_OEM_GRAPHICS_MODULES := \
    eglSubDriverAndroid \
    libEGL_adreno \
    libGLESv1_CM_adreno \
    libGLESv2_adreno \
    libq3dtools_adreno \
    libq3dtools_esx \
    libVkLayer_ADRENO_qprofiler \
    vulkan.adreno \
    libadreno_app_profiles \
    libadreno_utils \
    libCB \
    libgsl \
    libkcl \
    libkernelmanager \
    libllvm-glnext \
    libllvm-qcom \
    libllvm-qgl \
    libOpenCL \
    vendor.qti.qspmhal

# The OEM modules must be filtered inside gpu-q3.mk: inherit-product is
# recorded here and expanded later, so filtering in this file would run
# before the OEM module list exists.
$(call inherit-product, vendor/oneplus/sm8450-common/gpu-q3/gpu-q3.mk)
endif

# Mesa Turnip Vulkan driver (see BoardConfig GPU_DRIVER_TURNIP).
# The OEM vulkan.adreno.so stays installed, so switching back is a property
# change plus a rebuild: both files simply live side by side in
# /vendor/lib64/hw/ and ro.hardware.vulkan picks one.
# ro.hardware.vulkan is set here and nowhere else, so there is exactly one
# assignment and post_process_props stays happy. Turnip (freedreno) is the
# default since 2026-10-22; the OEM value (adreno) is the documented fallback
# and only changes which library the loader picks.
#
# GPU_DRIVER_TURNIP must be defaulted HERE as well, not only in BoardConfig.mk.
# BoardConfig and the product config are two separate make passes, and unlike
# GPU_DRIVER_Q3 this switch has no soong_config_set channel, so a BoardConfig
# default never reaches this file -- the ifeq below silently took the adreno
# branch and `get_build_var` still printed true from the board pass. `?=` keeps
# the environment override (GPU_DRIVER_TURNIP=false brunch ovaltine) working.
GPU_DRIVER_TURNIP ?= true
ifeq ($(GPU_DRIVER_TURNIP),true)
PRODUCT_PACKAGES += vulkan.freedreno
PRODUCT_VENDOR_PROPERTIES += ro.hardware.vulkan=freedreno
else
PRODUCT_VENDOR_PROPERTIES += ro.hardware.vulkan=adreno
endif

# Per-application Vulkan driver selector (privileged system app, no root needed).
PRODUCT_PACKAGES += VulkanDriverSwitch

# Fingerprint proxy HAL support.
# The original OPlus HAL is patched to register as IFingerprint/oplusfp (two
# instructions in its main(), pointing at a new string in a padding area of
# rodata) so this proxy can own IFingerprint/default. LD_PRELOAD was tried
# first and does not work: the environment variable is set correctly but the
# linker never maps the library.
PRODUCT_PACKAGES += \
    android.hardware.biometrics.fingerprint-service.proxy
