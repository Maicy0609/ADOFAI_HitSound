# ADOFAI HitSound Generator

将 `.adofai` 谱面转换为**连续打击音效 WAV** 的高性能工具。

![平台](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)
![语言](https://img.shields.io/badge/C%2B%2B-17-blue)
![构建](https://img.shields.io/badge/build-MSVC%20%7C%20GCC%20%7C%20Clang-informational)
![外部依赖](https://img.shields.io/badge/外部依赖-无%20DLL-brightgreen)
![许可证](https://img.shields.io/badge/license-Apache--2.0-green)

- **GitHub**：[@Maicy0609](https://github.com/Maicy0609)
- **仓库**：[ADOFAI_HitSound](https://github.com/Maicy0609/ADOFAI_HitSound)
- **Bilibili**：[我要丸冰火](https://space.bilibili.com/630056484)

---

## 目录

- [特性](#特性)
- [快速开始](#快速开始)
- [响度归一化](#响度归一化可选内置)
- [编译](#编译)
- [性能](#性能)
- [常见问题](#常见问题)
- [第三方组件与许可证](#第三方组件与许可证)
- [更新记录](#更新记录)

## 特性

| 特性 | 说明 |
| --- | --- |
| 奈奎斯特过滤 | 自动丢弃间隔小于 `1/(sr/2)`（约 41.7 µs @48k）的重复 hit，消除混叠伪影 |
| 静态等功率预缩放 | 按最大同时发声密度取常数增益 `1/√N`，全轨恒定增益、不做动态压缩 |
| 峰值安全余量 | 常数增益之外再留约 0.2 dB 余量，必要时按总量峰值兜底，不引入动态处理 |
| 事件全覆盖 | BPM 变速、Twirl、Hold、Pause 等事件按毫秒精度累积 |
| 两种谱面格式 | 同时支持 `angleData` 与 `pathData` |
| 内置响度归一化 | EBU R128（LUFS + 真峰值），**不需要任何外部 DLL** |
| 跨平台 | MSVC / GCC / Clang 同一份源码；Windows、Linux、macOS |

## 快速开始

### 下载

从 [Releases](https://github.com/Maicy0609/ADOFAI_HitSound/releases) 下载最新版本，或直接取这两份文件放到同一目录：

- [HitSound.exe](https://github.com/Maicy0609/ADOFAI_HitSound/raw/main/x64/Release/HitSound.exe)
- [hit.wav](https://github.com/Maicy0609/ADOFAI_HitSound/raw/main/x64/Release/hit.wav)

### 使用

1. 把 `HitSound.exe` 与 `hit.wav` 放在一起（也可以放在当前目录或 `x64/Release/`，程序会依次查找）
2. 运行 `HitSound.exe`，拖入或粘贴 `.adofai` 谱面文件路径
3. 等待合成完成，输出 WAV 与谱面同目录、同名

命令行用法：

```bash
HitSound.exe                      # 交互式：提示输入谱面路径
HitSound.exe /path/to/hit.wav     # 可选：第一个参数指定打击音样本
```

> 打击音的查找顺序：**命令行参数 → 可执行文件同目录 → 当前目录 → `x64/Release/`**。

## 响度归一化（可选，内置）

合成结束后会询问是否做响度平衡，按 `y` 启用，默认目标 **-23.0 LUFS**（EBU R128），
输出 `<谱面名>_norm.wav`。**不需要外部 DLL**：响度测量用的是直接编进程序的
[libebur128](https://github.com/jiixyj/libebur128)（MIT，见 `third_party/ebur128/`）。

做的事情就是 FFmpeg `loudnorm` 滤镜的 linear（常数增益）模式：

1. 测整轨的综合响度 `I`（LUFS）、响度范围（LU）与**真峰值**（dBTP）；
2. `gain = -23 − I`；若 `真峰值 + gain` 超过 -2 dBTP，则把增益压到真峰值上限；
3. 用同一个常数增益缩放全轨并写盘（不做动态 AGC，避免 pumping）。

## 编译

### MSVC（Windows）

```bat
cl /nologo /std:c++17 /O2 /EHsc HitSound.cpp third_party\ebur128\ebur128.c ^
   /Ithird_party\ebur128 /D_USE_MATH_DEFINES
```

### GCC / Clang（Linux、macOS，或 Windows 上的 MinGW）

```bash
gcc -O2 -Ithird_party/ebur128 -c third_party/ebur128/ebur128.c -o ebur128.o
g++ -std=c++17 -O2 -o HitSound HitSound.cpp ebur128.o -lm
```

x86-64 上如果想消掉 libebur128 那句 FTZ 提示，可以再加 `-msse2 -mfpmath=sse`
（不加也只是走它自带的手动 FTZ 分支，结果一致）。

依赖全部随仓库提供，无需额外安装：[RapidJSON](https://github.com/Tencent/rapidjson)（MIT）与
[libebur128](https://github.com/jiixyj/libebur128)（MIT）。出处与校验值见
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)。

## 性能

Tempest（158,403 tiles / 153,414 hits，过滤后最大同时发声 879）实测：

| 环境 | 阶段 | 耗时 |
| --- | --- | ---: |
| ubuntu-latest，4 核，GCC 13 `-O2` | 读谱 + 累积 | 88 ms |
| | 滤波 + 密度统计 + 混音 + 写盘（13.3 MB 单声道 WAV） | 524 ms |
| | **合计** | **612 ms** |
| windows-latest，4 核，MSVC 19.51 `/O2` | **合计** | **1.29 s** |

> 2023 年初版（16-bit 逐样本硬削波混音）在同一张谱面上需要约 15~30 秒（i5-9600T 单线程）；
> 现在的静态等功率方案把混音降成每样本一次 `double` 累加，快了一个数量级以上。
> 同一台机器上，另一个把每个样本 clamp 两次的实现（ADOCO 的 `HitsoundManager`）耗时 3.37 s，
> 对比基准见 [HitSoundBench](https://github.com/Maicy0609/HitSoundBench)。

## 常见问题

**Q：`_norm.wav` 打不开 / 播放器报错？**
是旧版本在 GCC 下写 WAV 头的一个越界 bug（`fmt` 块长度字段被声明成 `uint16_t` 却写 4 字节）。
已经修好；请用最新版本重新生成。

**Q：谱面里打击音很稀疏的段落听起来很小声？**
这是等功率预缩放的固有取舍：整轨用同一个常数增益，密集段的峰值决定了增益上限。
想要整轨响度统一，请启用响度归一化。

**Q：支持自定义打击音吗？**
支持。`hit.wav` 支持 16/24/32-bit 整数 PCM、任意采样率，混单声道使用。

## 第三方组件与许可证

本项目遵循 [Apache-2.0](LICENSE)。随仓库分发的第三方代码：

| 组件 | 位置 | 许可证 | 用途 |
| --- | --- | --- | --- |
| RapidJSON | `rapidjson/` | MIT | 解析 `.adofai` 的 JSON |
| libebur128 | `third_party/ebur128/` | MIT | EBU R128 响度测量（含真峰值） |

两者都是 MIT，**没有静态链接传染性问题**；`third_party/ebur128/sys/queue.h` 是本项目自写的
`<sys/queue.h>` 最小替身（MSVC 没有这个头），同样适用 Apache-2.0。详细出处、commit 与
sha256 校验值见 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)。

## 更新记录

**当前版本**

1. 跨平台：去掉 Windows 专属调用（`GetModuleFileNameA`/`shlwapi`/控制台代码页），
   同一份源码在 MSVC、GCC、Clang 下都能编（Linux/macOS 需要自己编一次，见上文）
2. 响度归一化改为内置 libebur128（MIT），**删掉 `AudioLoudnorm.dll` 等 5 个 DLL 的依赖**，
   并改用真峰值做上限
3. 修一个 GCC 下才暴露的 WAV 头越界写（见常见问题）
4. `hit.wav` 查找顺序扩展，支持命令行指定

**历史**

- 加入一键响度归一化；改用奈奎斯特过滤 + 静态等功率预缩放，解决高 BPM 谱面稀疏段声音小的问题
- 支持 `angleData` / `pathData` 两种谱面格式
