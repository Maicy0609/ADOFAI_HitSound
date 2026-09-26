# ADOFAI HitSound Generator

将 `.adofai` 谱面文件转换为连续打击音效 WAV 的高性能工具。

## 作者

- **Github**：[Maicy0609](https://github.com/Maicy0609)
- **仓库**：[ADOFAI_HitSound](https://github.com/Maicy0609/ADOFAI_HitSound)
- **Bilibili**：[我要丸冰火](https://space.bilibili.com/630056484)

## 快速开始

### 下载

从 [Releases](https://github.com/Maicy0609/ADOFAI_HitSound/releases) 下载最新版本，或直接下载以下文件到同一目录：

- [HitSound.exe](https://github.com/Maicy0609/ADOFAI_HitSound/raw/main/x64/Release/HitSound.exe)
- [hit.wav](https://github.com/Maicy0609/ADOFAI_HitSound/raw/main/x64/Release/hit.wav)

### 使用方法

1. 将 `HitSound.exe` 和 `hit.wav` 放在同一文件夹
2. 运行 `HitSound.exe`，拖入或粘贴 `.adofai` 谱面文件路径
3. 等待合成完成，生成的 `.wav` 文件位于谱面同目录
4. 可选：输入 `y` 进行 EBU R128 响度归一化（需额外 DLL，见下文）

### 响度归一化（可选）

启动时按 `y` 启用，默认目标 **-23.0 LUFS**（EBU R128）。**不需要任何外部 DLL**：
响度测量用的是直接编进程序里的 FFmpeg `libavfilter/ebur128.c`（见 `third_party/ffmpeg/`，
出处与许可证见 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)）。

做的事就是 FFmpeg `loudnorm` 滤镜的 linear（常数增益）模式：

1. 测整轨的综合响度 `I`（LUFS）与采样峰值；
2. `gain = -23 − I`，若 `峰值 + gain` 超过 -2 dBTP 就把增益压到峰值上限；
3. 用同一个常数增益缩放全轨（不做动态 AGC，避免 pumping）。

> 峰值上限用的是**采样峰值**而不是真峰值：采样峰值恒 ≤ 真峰值，所以这个上限偏保守，
> 不会过冲（FFmpeg 的 loudnorm 能报真峰值是另外走了一套 libswresample 上采样，这里不引入）。

## 特性

- 奈奎斯特过滤：自动丢弃间隔 < 1/(sr/2) 的重复 hit，消除混叠伪影
- 静态等功率预缩放：基于最大同时发声密度，恒定增益无动态压缩
- 支持 BPM 变速、Twirl、Hold、Pause 等全部 ADOFAI 事件
- 支持 `angleData` 和 `pathData` 两种谱面格式
- 毫秒级精度时间轴计算
- 内置响度归一化后处理（EBU R128）

## 编译

MSVC（Windows）：

```bash
cl /std:c++17 /O2 /EHsc HitSound.cpp ^
   third_party\ffmpeg\ebur128\ebur128.c /D_USE_MATH_DEFINES
```

GCC / Clang（Linux、macOS，或 Windows 上的 MinGW）：

```bash
gcc -O2 -c third_party/ffmpeg/ebur128/ebur128.c -o ebur128.o
g++ -std=c++17 -O2 -o HitSound HitSound.cpp ebur128.o -lm
```

依赖：全部随仓库提供，不需要额外安装（出处与许可证见 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)）

- [RapidJSON](https://github.com/Tencent/rapidjson)（MIT，`rapidjson/` 目录）
- FFmpeg 的 `libavfilter/ebur128.c`（EBU R128 响度测量，LGPL-2.1+，`third_party/ffmpeg/` 目录）

程序按「命令行参数 → 可执行文件同目录 → 当前目录 → `x64/Release/`」的顺序找 `hit.wav`，
也可以直接把路径当第一个参数传进去：`./HitSound /path/to/hit.wav`。

## 测试谱面

[Tempest.adofai](https://github.com/Maicy0609/ADOFAI_HitSound/raw/main/x64/Release/Tempest.adofai) — 作者 @StArray 仅供性能测试。

- 158,403 tiles
- 153,414 hits（过滤后）
- 最大同时发声密度：879

## 性能

测试环境：Intel Core i5-9600T @ 2.30GHz，单线程

| 谱面 | Tiles | 耗时 |
|------|-------|------|
| 158k tiles | 158,403 | ~15–30 s |

## 注意事项

- 需要 Visual C++ 2022 运行库（x64）
- `hit.wav` 支持 16/24/32-bit 整数 PCM，采样率任意
- 输出为 44100 Hz 16-bit 单声道 WAV

## 主要更新：
>1. 加了作者信息和三个链接
>2. 补充了奈奎斯特过滤、等功率预缩放等核心特性说明
>3. 加了响度归一化的 DLL 使用说明
>4. 更新了测试数据（158k tiles 的 Tempest 谱面）
>5. 整体结构重新组织，阅读更清晰