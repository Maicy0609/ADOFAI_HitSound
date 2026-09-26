# v2.0

跨平台 + 去掉外部 DLL，并修掉三个会算错/写坏文件的 bug。

## 下载

| 平台 | 包 |
| --- | --- |
| Windows x64 | `HitSound-v2.0-windows-x64.zip` |
| Linux x64（静态链接，不挑发行版） | `HitSound-v2.0-linux-x64.tar.gz` |
| Linux arm64（静态链接，树莓派 / ARM 服务器 / Apple Silicon 虚拟机） | `HitSound-v2.0-linux-arm64.tar.gz` |
| macOS（universal：Intel + Apple Silicon，最低 macOS 11） | `HitSound-v2.0-macos-universal.tar.gz` |

每个包里都有可执行文件、`hit.wav`、README、许可证与第三方声明。

## 本次改动

### 1. 跨平台（同一份源码）

- 去掉 Windows 专属调用：取自身路径改为按平台各用一条系统调用（Windows `GetModuleFileNameA` /
  Linux `/proc/self/exe` / macOS `_NSGetExecutablePath`），`windows.h`、`shlwapi`、控制台代码页
  全部收进 `#ifdef`。MSVC、GCC、Clang 都能编。
- `hit.wav` 查找顺序：命令行参数 → 可执行文件同目录 → 当前目录 → `x64/Release/`。

### 2. 响度归一化不再需要外部 DLL

原来要 `AudioLoudnorm.dll` + 4 个 FFmpeg DLL；现在把 **MIT 许可的 libebur128** 直接编进程序：

- 采用 FFmpeg `loudnorm` 滤镜的 **linear（常数增益）** 模式：`gain = -23 − I`，并以**真峰值**
  （-2 dBTP）为上限；不做动态 AGC，避免 pumping。
- 出处、commit 与 sha256 校验值见 `THIRD-PARTY-NOTICES.md`（RapidJSON 与 libebur128 都是 MIT）。

### 3. 修掉三个 bug

| bug | 现象 | 原因 |
| --- | --- | --- |
| 旋转量归一化 | 某些谱面**整段击打偏早、个别击打消失** | `da = 180 - angle + prev` 只做一次 `if` 加减，遇到 `angleData` 里的负数 / 大于 360 的值修不回来（最小复现 `[-112.5, 472.5, 0]`）；已改为 `fmod` 取模 |
| 同一 floor 上的 `Bpm` + `Multiplier` 事件 | 那一段 BPM 差 4.5 倍，**整关时长少 11.9 秒** | 乘数事件覆盖掉了绝对 BPM（`stdbpm = -multiplier`），变成"上一 tile × 乘数"；已改为连乘 `stdbpm *= multiplier` |
| WAV 头越界写 | GCC 编出来的 `_norm.wav` 播放器打不开 | `fmt` 块长度字段声明成 `uint16_t` 却按 4 字节写出；已改为 `uint32_t` |

以上都有实测复现与验证：6.77M tiles 的压测关卡上，修复前时间轴总长 194.2442 s、比正确值少
11.935 s；修复后 206.1792 s，与对照实现逐 tile 差 ≤ 0.0011 ms。对比数据见
[HitSoundBench](https://github.com/Maicy0609/HitSoundBench)。

### 4. 新增 `settings.offset` 支持（默认开启）

把谱面的歌曲偏移（毫秒）叠加到所有击打时间上，输出音轨可以直接铺在从 0 播放的歌曲上；
`offset = 0` 的谱面行为与旧版**逐字节一致**。需要旧行为用 `--no-offset`。

### 5. 新增 `--no-nyquist`：可关闭奈奎斯特过滤

默认仍启用过滤（丢弃间隔 < 41.7 µs @48k 的重复 hit，避免堆叠成糊墙）；加 `--no-nyquist`
则**保留谱面上的每一个击打**。关掉后最大同时发声数变大，`1/√N` 静态等功率预缩放会自动
跟着调整，不会削波。极密谱面差异很大（某 6.77M tiles 压测关卡：开 = 57 万 hits，关 = 677 万 hits）。

## 用法

```bash
HitSound                  # 交互式：提示输入 .adofai 路径
HitSound /path/to/hit.wav # 指定打击音样本
HitSound --no-offset      # 不叠加 settings.offset
HitSound --no-nyquist     # 关闭奈奎斯特过滤，保留全部击打
```

输出 WAV 与谱面同目录、同名；按 `y` 可再做 EBU R128 响度平衡，输出 `<谱面名>_norm.wav`。

## 校验（Linux x64 静态包）

```
$ ./HitSound --help 2>/dev/null; file HitSound      # ELF 64-bit, statically linked
$ printf 'level.adofai\nn\n' | ./HitSound
```
