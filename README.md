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

如需对输出 WAV 进行 EBU R128 响度平衡，请将以下 5 个 DLL 与 exe 同目录放置：

- `AudioLoudnorm.dll`
- `avutil-60.dll`
- `swresample-6.dll`
- `avfilter-11.dll`
- `avcodec-62.dll`

> 启动时按 `y` 即可启用，默认目标 -23.0 LUFS。

## 特性

- 奈奎斯特过滤：自动丢弃间隔 < 1/(sr/2) 的重复 hit，消除混叠伪影
- 静态等功率预缩放：基于最大同时发声密度，恒定增益无动态压缩
- 支持 BPM 变速、Twirl、Hold、Pause 等全部 ADOFAI 事件
- 支持 `angleData` 和 `pathData` 两种谱面格式
- 毫秒级精度时间轴计算
- 内置响度归一化后处理（EBU R128）

## 编译

```bash
cl /std:c++17 /O2 /arch:AVX2 /EHsc HitSound.cpp /I./rapidjson
```
依赖：[RapidJSON](https://github.com/Tencent/rapidjson)（header-only，放入 `rapidjson/` 目录即可）

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