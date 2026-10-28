# Vulkan ICD 切换（不改系统框架）

目标：Vulkan 驱动（OEM `vulkan.adreno.so` / Mesa `vulkan.freedreno.so`）在运行期按应用切换，
**不修改 `frameworks/native`**，换 ROM、重编框架都不丢。

## 为什么旧方案要改框架

`frameworks/native/vulkan/libvulkan/driver.cpp` 的 `LoadDriver()` 只有一条路径：

```cpp
std::string lib_name = android::base::GetProperty("ro.hardware.vulkan", "");
lib_name = "vulkan." + lib_name + ".so";          // 唯一入口
so = android_load_sphal_library(lib_name.c_str(), LIB_DL_FLAGS);
```

`ro.hardware.vulkan` 是 `ro.*` 只读属性 → 构建期定死，运行期不可改。于是旧做法只能在
`driver.cpp` 里插一段 per-app 查表逻辑（`perapp-vulkan-driver.patch`），这属于系统框架补丁：
framework 一更新、ROM 一换，补丁就得重新维护。

## 新方案：把开关做进 HAL 层

loader 的契约只有三步（`frameworks/native/vulkan/libvulkan/driver.cpp`）：

1. 打开 `vulkan.<ro.hardware.vulkan>.so`；
2. `dlsym(so, "HMI")` 拿 `hw_module_t`，检查 `id == "vulkan"`；
3. 调 `module->common.methods->open(module, "vk0", &device)`，之后只用 `hwvulkan_device_t`
   里的三个函数指针（`EnumerateInstanceExtensionProperties` / `CreateInstance` / `GetInstanceProcAddr`）。

所以把 `ro.hardware.vulkan` 固定成 `switcher`，让 `vulkan.switcher.so` 当那个 HAL 模块：

```
libvulkan (stock)
   └─ dlopen vulkan.switcher.so        ← ro.hardware.vulkan=switcher（构建期一次，永不再改）
        ├─ 读 /data/misc/vulkan/per_app.conf（可写，优先）
        ├─ 读 /vendor/etc/vulkan/per_app.conf（镜像默认）
        └─ dlopen vulkan.adreno.so / vulkan.freedreno.so，把它的 hwvulkan_device_t 原样交出
```

`libvulkan` 一行没改，两个 ICD 都留在 `/vendor/lib64/hw/` 里，切换 = 改一行配置 + 重启那个进程。

## 文件清单

新增 `device/oneplus/ovaltine/vulkan-switcher/`：

| 文件 | 作用 |
|---|---|
| `vulkan_switcher.cpp` | HAL 模块实现：选 ICD、加载、转发 device 结构 |
| `Android.bp` | `cc_library_shared`，输出 `/vendor/lib64/hw/vulkan.switcher.so` |
| `per_app.conf.turnip` | 镜像默认配置（`*=freedreno`，Turnip 优先） |
| `per_app.conf.oem` | 镜像默认配置（`*=adreno`，OEM 优先） |
| `apply-integration.py` | 把下面这些改动一次性打进树里（幂等） |

设备树改动：

| 文件 | 改动 |
|---|---|
| `device/oneplus/ovaltine/device.mk` | 装 `vulkan_switcher`；`ro.hardware.vulkan=switcher`；`GPU_DRIVER_TURNIP` 只决定镜像默认配置 |
| `device/oneplus/sm8450-common/vendor.prop` | 删掉重复的 `ro.hardware.vulkan=adreno`（属性只能有一处赋值） |
| `device/oneplus/sm8450-common/sepolicy/file_contexts` | 给 `vulkan.switcher.so` 打 `same_process_hal_file`；`/vendor/etc/vulkan` 打 `vulkan_perapp_conf_file` |
| `device/oneplus/sm8450-common/sepolicy/vulkan_perapp.te` | 追加 `allow su` 写权限（adb root 直接改映射） |
| `device/oneplus/sm8450-common/init/init.qcom.rc` | `mkdir /data/misc/vulkan 0770 system system` |
| `device/oneplus/sm8450-common/common.mk` | 4 处布尔 `soong_config_set` → `soong_config_set_bool`（hardware/oplus 的 bp 用了布尔 select 键，类型不符会卡 soong） |

## 配置语法

```
# /data/misc/vulkan/per_app.conf  或  /vendor/etc/vulkan/per_app.conf
<package>=<subname>
*= <subname>
```

* 精确包名优先于 `*`；`/data` 优先于 `/vendor/etc`。
* `subname` 只能是 `[a-z0-9_-]`（防路径注入），未知/写错一律忽略。
* 候选顺序：配置 → `/vendor/etc` 的 `*` → 硬编码 `adreno` → `freedreno`，
  任一 ICD 打不开就自动换下一个，最坏退回 stub HAL，不会让进程没有 Vulkan。

常用操作（`adb root`）：

```bash
# 全局换成 Turnip
adb shell 'echo "*=freedreno" > /data/misc/vulkan/per_app.conf'
# 只给某个 app 换回 OEM 驱动
adb shell 'echo "com.tungsten.fcl=adreno" >> /data/misc/vulkan/per_app.conf'
# 生效：重启对应进程
adb shell am force-stop com.tungsten.fcl
```

验证：

```bash
adb logcat -s vkswitch                       # 应为 "pkg=<包名> configured=... -> freedreno"
adb shell dumpsys gpu | grep -i -A3 vulkan   # 看当前进程实际用的 ICD
```

## 验证状态

* 代码对树里头文件编译通过（`clang-r563880c`，`--target=aarch64-linux-android34`，
  `-Wall -Wextra`，仅剩 `missing field 'dso' initializer` 提示，属静态零初始化，与 `nulldrv` 一致）。
* 链接产物 `HMI` 以 **default visibility (`D`) 导出**，正是 loader `dlsym(so, "HMI")` 需要的符号。
* 实机行为（加载哪个 ICD、per-app 生效）尚未验证 —— 整包构建目前被下面的环境问题挡住。

## 风险与回滚

* 风险：`vulkan.switcher.so` 自己崩会让 `libvulkan` 拿不到 HAL（回退 stub，等于没有 Vulkan）。
  缓解：只 `dlopen` + 转发，不碰图形状态；两个 ICD 都失败才返回失败。
* 风险：SELinux 若拒绝读取配置文件，只会退回镜像默认值，不会崩。
* 回滚：`ro.hardware.vulkan` 改回 `freedreno` 或 `adreno` 并重编，即回到"构建期单一驱动"的旧行为；
  shim 与配置文件留着也无害。

## 当前阻塞（与本改动无关）

`m vulkan_switcher` 卡在 soong 全树解析：

```
error: vendor/lineage/build/soong/Android.bp:21:1: module "generated_kernel_includes":
       source path kernel/oneplus/sm8450/include/soc/oplus/... does not exist
```

`kernel/oneplus/sm8450/include/soc/oplus/` 下这些头是符号链接，指向
`kernel/oneplus/gki-modules/oplus/kernel/...`，而该目录不存在 —— 从 9repo bundle 恢复时
`kernel_oneplus_gki-modules.bundle` 是浅包（`fatal: remote did not send all necessary objects`），
没能 clone 出来。补齐这个仓之后 `m vulkan_switcher` / 整包才能继续。
