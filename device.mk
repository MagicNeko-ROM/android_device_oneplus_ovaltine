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
# The HIDL 2.3 service from hardware/oplus/hidl/fingerprint is NOT installed:
# its instance is missing from the VINTF manifest, so it SIGSEGVs at boot
# (/vendor/bin/hw/android.hardware.biometrics.fingerprint@2.3-service.oplus).
# Fingerprint is served by the AIDL proxy in device/oneplus/ovaltine/fingerprint,
# which forwards to the stock AIDL HAL registered as IFingerprint/oplusfp.
# PRODUCT_PACKAGES += \
#     android.hardware.biometrics.fingerprint@2.3-service.oplus

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
    frameworks/native/data/etc/com.nxp.mifare.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/com.nxp.mifare.xml \
    vendor/oneplus/sm8450-common/proprietary/vendor/etc/libnfc-nxp.conf:$(TARGET_COPY_OUT_VENDOR)/etc/libnfc-nxp.conf \
    vendor/oneplus/sm8450-common/proprietary/vendor/etc/libnfc-nci.conf:$(TARGET_COPY_OUT_VENDOR)/etc/libnfc-nci.conf \
    vendor/oneplus/sm8450-common/proprietary/vendor/firmware/sn100u.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/sn100u.bin

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

# vendor/extra (LineageOS community extras): PRODUCT_ADB_KEYS whitelists a
# public key for adbd, overlay-lineage ships a few UI defaults, iperf3/tinymix
# are handy on-device tools. ih8sn and GMS stay off unless asked for.
$(call inherit-product-if-exists, vendor/extra/product.mk)

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

# Vulkan ICD switching without touching the framework.
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

PRODUCT_PACKAGES += \
    vulkan_switcher \
    vulkan-switch \
    VulkanSwitch

# adb root on a fresh flash; the developer option still overrides it.
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += persist.sys.root_access=3

# /data/misc/vulkan must be created by the init domain, so this rc goes on the
# system partition instead of next to the other vendor bits.
PRODUCT_COPY_FILES += \
    device/oneplus/ovaltine/vulkan-switcher/vulkan_perapp.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/vulkan_perapp.rc

PRODUCT_VENDOR_PROPERTIES += ro.hardware.vulkan=switcher

ifeq ($(GPU_DRIVER_TURNIP),true)
PRODUCT_PACKAGES += vulkan.freedreno
endif
# No /vendor/etc/vulkan/per_app.conf on purpose: sepolicy neverallow
# (domain.te:2155) forbids app domains from reading vendor_file_type files.
# The switcher's image default is compiled in; runtime overrides go to
# /data/misc/vulkan/per_app.conf.

# The privileged selector app that used to rewrite /data/misc/vulkan/per_app.conf
# was not carried over into this tree; naming it here would fail the build with
# an unknown module. Until it lands, use /vendor/bin/vulkan-driver (root shell)
# or edit the file directly.
# PRODUCT_PACKAGES += VulkanDriverSwitch

# Fingerprint proxy HAL support.
# The original OPlus HAL is patched to register as IFingerprint/oplusfp (two
# instructions in its main(), pointing at a new string in a padding area of
# rodata) so this proxy can own IFingerprint/default. LD_PRELOAD was tried
# first and does not work: the environment variable is set correctly but the
# linker never maps the library.
PRODUCT_PACKAGES += \
    libshims_aidl_fingerprint_v2.oplus

# The proxy is built against fingerprint-V4-ndk because V5 is the unfrozen
# current version and vendor modules may not depend on it. The framework however
# is compiled against fingerprint-V5-java, parses SensorProps with the V5 layout
# and dies with IllegalArgumentException inside BiometricServiceRegistry, which
# boot-loops system_server. The PJG SensorProps shim only rewrites field values,
# not the parcel layout, so it cannot paper over the mismatch.
# Re-enable only once the proxy speaks V5 (freeze aidl_api/.../5 first).
# PRODUCT_PACKAGES += \
#     android.hardware.biometrics.fingerprint-service.proxy
