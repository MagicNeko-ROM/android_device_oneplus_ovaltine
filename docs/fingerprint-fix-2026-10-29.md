# OnePlus 10T (ovaltine) 指纹完整修复记录

**日期**：2026-10-29
**设备**：OnePlus 10T / `ovaltine` / `OP5552L1` / `PGP110`，序列号 `734ce825`
**基线**：aviumui 16.2.2（lineage-23.2），`lineage_ovaltine-bp4a-userdebug`
**结果**：屏下光学指纹录入/解锁可用

---

## 一、症状演进（三个阶段）

| 阶段 | 现象 | 根因层 |
|---|---|---|
| A | 开机后 system_server 每几秒崩一次，无限重启 | framework 解析 `getSensorProps` 失败 |
| B | 系统能起，进设置点「添加指纹」立刻弹「无法完成指纹设置」 | HAL 接口版本/method 集不匹配 |
| C | 能进录入页、能采图，但报 `FP_ERR_FINGERUP_TOO_FAST`，屏幕变黑 | 面板 HBM 未触发 |

三个阶段是**串联**的：不修 A 进不了 B，不修 B 进不了 C。

---

## 二、根因链（共 7 项，逐项都是必要条件）

### 1. stock HAL 被人为 patch 成注册 `oplusfp`

厂商固件里 `/odm/bin/hw/vendor.oplus.hardware.biometrics.fingerprint@2.1-service_uff`
原本注册 `IFingerprint/default`（framework 只认这个名）。本树里那份是被人改过、
注册 `IFingerprint/oplusfp` 的版本（562,248 字节，sha256 `8b54f4af…`），配套改的还有
VINTF manifest 和 init rc 里的一行注释。

**修法**：换回未修改的原始二进制。

```
547,633 字节
sha256 d81177ee5199b677a5a5a2d4a250b5da622c7af1b5e39b857c05b8f4f0260252
```

### 2. VINTF manifest 声明 `oplusfp`

`odm/etc/vintf/manifest/manifest_oplus_fingerprint_aidl_v2.xml` 里
`<fqname>` 写的是 `IFingerprint/oplusfp`。

**修法**：改回 `IFingerprint/default`。

> 1 和 2 修完，`service list` 里就能看到
> `android.hardware.biometrics.fingerprint.IFingerprint/default` 了。
> 但 system_server 仍然崩 —— 这才露出第 3 项。

### 3. `SensorProps::writeToParcel` 序列化布局不匹配

HAL 是按旧版（A14 时代）AIDL 冻结快照编译的，而 framework 用当前定义解析。
这一层试过三条路，前两条都错：

| 做法 | 结果 |
|---|---|
| 值拷贝 `*thisptr` 后改写字段（lineage 上游原版） | 读到旧布局里的垃圾（`componentInfo` 的 vector 头），`-fno-exceptions` 下死在 `std::__throw_length_error` → SIGABRT |
| 原样透传（passthrough） | 写出旧布局字节，framework 按新布局读 → `FingerprintSensorConfigurations.getSensorPropForInstance` 抛 `IllegalArgumentException`，发生在 `BiometricServiceRegistry` 线程 → system_server 死循环 |
| **全新构造 + 补齐新字段**（最终方案） | 通过 |

**最终实现**：

```cpp
extern "C" binder_status_t
SensorProps::writeToParcel(SensorProps* /*thisptr*/, AParcel* parcel) {
    // 绝不读 *thisptr —— 它按 HAL 自己的旧头文件布局
    SensorProps props;                                  // 当前布局，默认初始化
    props.commonProps.sensorId = 0;
    props.commonProps.sensorStrength = SensorStrength::STRONG;
    props.commonProps.maxEnrollmentsPerUser = 5;
    props.supportsDetectInteraction = true;
    props.halControlsIllumination = true;
    SensorPropsInit(&props);                            // 从系统属性填 sensorType / locations

    int32_t start = AParcel_getDataPosition(parcel);
    v2Write(&props, parcel);                            // 交给 V2 序列化器写 7 个字段
    AParcel_writeBool(parcel, false);                   // 补当前定义新增的 nullable 字段
    int32_t finish = AParcel_getDataPosition(parcel);
    AParcel_setDataPosition(parcel, start);
    AParcel_writeInt32(parcel, finish - start);         // 回填块长度前缀
    AParcel_setDataPosition(parcel, finish);
    return STATUS_OK;
}
```

> **为什么不能链接 V4（current）**：HAL 是 V2，soong 拒绝同一模块图混用
> `fingerprint-V2-ndk` 和 `-V4-ndk`（`depends on multiple versions of the same aidl_interface`）。
> 所以只能用 V2 类型 + 手工补齐差异。

### 4. `writeToParcel` 返回类型写成 `void`

生成的签名是 `binder_status_t SensorProps::writeToParcel(AParcel*) const`，
状态通过 **w0** 返回。写成 `void` 时 w0 是垃圾值，HAL 当成失败：

```
W …-service_uff: PruneStatusT: Unknown status_t (1 (Unknown error -1))
                 pruned into STATUS_UNKNOWN_ERROR
```

framework 侧看到的则是 `java.lang.RuntimeException: Unknown error`。

**这一行 HAL 日志是整个排查的转折点** —— 它证明 shim 被调用了、数据也构造了，
只是返回值坏了。

### 5. 接口版本只覆盖了 `BnSession`，漏了 `BnFingerprint`

framework 取 HAL 接口版本的路径是：

```
FingerprintStartUserClient.java:62   final int version = hal.getInterfaceVersion();   // hal 是 IFingerprint
```

`AidlSession.hasContextMethods()` 就是 `mHalInterfaceVersion >= 2`。
版本报 2 → 框架调 `enrollWithContext` → 这个 HAL 没实现 → `DeadObjectException`。

**修法**：`BnFingerprint` 和 `BnSession` 的 `getInterfaceVersion` 都报 1，
`getInterfaceHash` 返回 `EX_UNSUPPORTED_OPERATION`。

### 6. 版本覆盖函数的 ABI 写错（SIGSEGV at 0x5）

`ndk::ScopedAStatus` 是**结构体返回值**，AAPCS64 把它的地址放在 **x8**。
最初的写法：

```cpp
extern "C" void f(void* this, int32_t* ver, AStatus** status)   // ← status 落在 x2
```

C 编译器把 `status` 排到第 3 个参数（x2），而调用方根本没往 x2 放东西 ——
它用的是 x8。于是 `*status = ...` 写到一个垃圾地址：

```
Fatal signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0x5
#00 pc 0x55dc  libshims_aidl_fingerprint_v2.oplus.so
     (BnFingerprint::getInterfaceVersion(int*)+36)
```

**修法**：让它成为 C++ 函数、按值返回 `ndk::ScopedAStatus`，
但用 `asm()` 把符号名钉成 mangled 名：

```cpp
ndk::ScopedAStatus
opGetFingerprintInterfaceVersion(void* /*thisptr*/, int32_t* version)
        asm("_ZN4aidl7android8hardware10biometrics11fingerprint13BnFingerprint19getInterfaceVersionEPi");

ndk::ScopedAStatus
opGetFingerprintInterfaceVersion(void* /*thisptr*/, int32_t* version) {
    if (version) *version = 1;
    return ndk::ScopedAStatus::ok();
}
```

> 不能直接写 `extern "C" ndk::ScopedAStatus …`：
> clang 会报 `-Wreturn-type-c-linkage` 且是 error 级别，
> `-Wno-return-type-c-linkage` 也压不住。

### 7. 面板无 local HBM，UdfpsExtension 缺失

DTSI 是 `oplus,ofp-fp-type = <0x08>`，**没有 BIT(4) local HBM** ——
不能只点亮指纹那一小块，只能整屏进 HBM，靠 SystemUI 的压暗层压出圆形观感。

设备树里已经调好了压暗层（`config_udfpsDimmingBrightnessAlphaArray`，4095 端点），
但 `hardware/qcom-caf/sm8450/display` 里**根本没有 `UdfpsExtension`**
（只有 sm8550 / sm8750 有），HBM 触发点不存在，于是：

```
hbmMode=off
[FP_HAL][OpticalCore]: checkFingerUpTooFast, FP_FINGER_UP FP_ERR_FINGERUP_TOO_FAST
[FP_HAL][IFingerprintCore]: captureImg:323: result:10108
```

屏幕被压暗、亮度没抬起来 → 采不到清晰图像 → 录入失败。

**修法**：应用 `device/oneplus/sm8450-common/upstream-patches/0001-display-sm8450-udfps-hbm-and-sdmext.patch`
（14 个文件，`git apply --check` 干净）。

---

## 三、修复 7 引发的回归：桌面卡死

应用 HBM 补丁后系统起不来，停在 `FallbackHome`，`livedisplay` 每 5 秒 abort 一次：

```
F vendor.lineage.livedisplay-impl-sdm: Failed to initialize SDM backend
F DEBUG: Abort message: 'Failed to initialize SDM backend'
PID 4735 → 4782 → 4829 → 4994（每 5 秒一次）
```

**根因**：`SDM` 只允许一个进程持有 DRM master。composer 先启动拿走它，
后启动的 livedisplay 必然失败。HBM 补丁改了 `core_impl.cpp`：

```diff
- if ((err != kErrorNone) || !enable_null_display_) {
+ if (err != kErrorNone) {
```

这让 composer 的降级路径生效（本来是对的），但 livedisplay 走同一条路时拿到
null display，后续 `CreateCore` 依然失败，而 `SDMController.cpp` 那边是：

```cpp
if (init() != android::OK) {
    LOG(FATAL) << "Failed to initialize SDM backend";   // ← abort
}
```

于是 init 反复重拉 → 崩溃循环 → Launcher 抢不到前台。

**修法**（`hardware/lineage/interfaces/livedisplay/aidl/sdm/SDMController.cpp`）：
livedisplay 只是显示调节，属可选组件，初始化失败应降级而非 abort。
`CONTROLLER_CHECK` 宏本来就检查 `hctx_ == 0`，降级是安全的。

```cpp
if (init() != android::OK) {
    LOG(ERROR) << "Failed to initialize SDM backend, display control unavailable";
    hctx_ = 0;
    return;
}
```

> 注意 `hctx_` 是 `uint64_t` 不是指针，写 `nullptr` 会编译失败。

---

## 四、部署过程

### 4.1 编译

```bash
cd /root/aviumui
source build/envsetup.sh
lunch lineage_ovaltine-bp4a-userdebug
m bacon -j32          # 约 11–14 分钟（热缓存）
```

### 4.2 拉包（必须用新文件名，不覆盖旧包）

```bash
rsync -a --info=progress2 lupa:/root/aviumui/out/target/product/ovaltine/AviumUI-*.zip \
      ./AviumUI-16.2.2-ovaltine-20261029-hbm-sdmfix.zip
sha256sum AviumUI-16.2.2-ovaltine-20261029-hbm-sdmfix.zip | tee *.sha256
# f3f88598e8f7c8a009a13a0ecdaa6c8456dbcf62962244e1d68dae99b8a27421
```

### 4.3 刷入（只能 sideload，不能 fastboot flash）

```bash
adb reboot sideload
adb -s 734ce825 sideload AviumUI-16.2.2-ovaltine-20261029-hbm-sdmfix.zip   # 约 4–5 分钟
adb -s 734ce825 reboot
```

> **`fastboot flash vendor_a` 无效**：super 上有 dm-snapshot 的 COW 残留
> （`vendor_a-cow`），写进去不生效，连 `delete-logical-partition vendor_a-cow`
> 都清不掉。只有 full OTA sideload 走 update_engine 才真正落盘。

### 4.4 验证

```bash
adb shell getprop sys.boot_completed          # 1
adb shell getprop ro.build.version.incremental # 1793243681（与包内 post-build-incremental 一致）
adb shell service list | grep fingerprint     # IFingerprint/default 在
adb logcat -b crash -d | grep -c "SDM backend"   # 0
adb logcat -b crash -d | grep -c "FATAL EXCEPTION"  # 0
adb shell dumpsys biometric | head            # 列出 ID(1008) modality 8
```

### 4.5 快速迭代技巧（省掉整包 sideload）

不需要重启的模块可以热替换，全程约 2 分钟而不是 10 分钟：

```bash
# 只编目标模块
m libshims_aidl_fingerprint_v2.oplus -j32

# 推到设备并覆盖
adb root
adb push libshims_aidl_fingerprint_v2.oplus.so /data/local/tmp/x.so
adb shell "chcon u:object_r:vendor_file:s0 /data/local/tmp/x.so"
adb shell "mount --bind /data/local/tmp/x.so /odm/lib64/libshims_aidl_fingerprint_v2.oplus.so"
adb shell "setprop ctl.restart fps_hal"
```

**适用**：指纹 HAL shim（`fps_hal` 可安全重启）。
**不适用**：`display composer` —— 它是 SurfaceFlinger 的上游，
`ctl.stop` 之后 `ctl.start` 起不来，整屏黑；而且 bind mount 重启即失效。
display 层改动只能整包刷。

---

## 五、文件清单

| 仓库 | commit | 文件 |
|---|---|---|
| `hardware/oplus` | `ae0e624` | `fingerprint/shims/SensorPropsShim.cpp`、`fingerprint/shims/Android.bp` |
| `hardware/lineage/interfaces/livedisplay` | `16eabf1` | `aidl/sdm/SDMController.cpp` |
| `hardware/qcom-caf/sm8450/display` | `0489f4cfa5` | 14 个（UdfpsExtension + SDM 扩展钩子） |
| `device/oneplus/ovaltine` | `303366b` | `device.mk`（停用 proxy / HIDL 服务）、`fingerprint/FingerprintProxy.cpp` |
| `vendor/oneplus/ovaltine` | `e02766c` | stock HAL 二进制、`manifest_oplus_fingerprint_aidl_v2.xml`、init rc |

全部推送到 `github.com/MagicNeko-ROM/*`，分支 `lineage-23.2`。
其中 `android_hardware_qcom_display` 与 `android_hardware_lineage_interfaces`
原不在该组织下，为本次新建。

---

## 六、排障方法沉淀

1. **先看 HAL 自己打的日志**。`PruneStatusT: Unknown status_t` 这一行比 framework
   的异常堆栈有用得多 —— 它指明了错误发生在 HAL 侧且是返回值问题。
2. **tombstone 的 `#00` 帧决定一切**。`BnFingerprint::getInterfaceVersion+36`
   直接把范围缩到那一个函数。
3. **对比"能用"的包**（本次用了 crDroid v12.12-livefix）。但要注意结论边界：
   crDroid 的 shim 之所以能工作，是因为**它的 framework 读的也是 V2**；
   移植到 V4 的树上时，同样的做法会失效 —— 必须让 HAL 写 current 布局。
4. **`-fno-exceptions` 下任何 `std::` 抛异常都是 abort**。
   值拷贝一个布局不匹配的结构体，代价就是 SIGABRT。
5. **改完 HAL 先确认 ABI**：返回值走哪个寄存器、结构体返回走 x8。
   函数体逻辑全对，签名错了照样崩。
6. **降级路径的语义要想清楚**：SDM 的 null display 对 composer 是降级，
   对 livedisplay 是致命 —— 同一个 `Init()` 失败对不同调用者含义不同。
