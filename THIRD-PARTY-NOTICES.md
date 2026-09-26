# 第三方组件与许可证

本项目自身（`HitSound.cpp`、`Hitsound.py` 等）沿用仓库根目录的 [LICENSE](LICENSE)（Apache-2.0）。
下面这些第三方代码随仓库一起分发，各自适用**它们自己的**许可证：

## 1. RapidJSON —— MIT

- 位置：`rapidjson/`
- 来源：https://github.com/Tencent/rapidjson
- 用途：解析 `.adofai` 谱面 JSON

## 2. FFmpeg 的 EBU R128 实现 —— LGPL-2.1-or-later

- 位置：`third_party/ffmpeg/ebur128/ebur128.{c,h}`（**逐字节原样，未做任何修改**）
- 来源：FFmpeg，分支 `release/9.0`，commit `51c4a23d74ac2fbf2fb066e0ce274a966a16da23`
  - 上游路径：`libavfilter/ebur128.c`、`libavfilter/ebur128.h`
  - 校验：`ebur128.c` sha256 `bce02f6c8f0518d538596c7c46c6bbce68e5f50042f68a17cc04e6c533d1a459`
  - `ebur128.h` sha256 `891e45637d56cd6292633d865564dda03ae5059fe471d54eceeebb01eccb583f`
- 许可证副本：`third_party/ffmpeg/COPYING.LGPLv2.1`（该文件头部同时保留了其上游 libebur128 的 MIT 授权声明）
- 用途：响度归一化的 EBU R128 测量（综合响度 / 响度范围 / 采样峰值）

`third_party/ffmpeg/ebur128/libavutil/*.h` 这 5 个小文件**不是** FFmpeg 的代码，是本项目自己写的
最小替身（`AVERROR`/`FFMAX`/`av_malloc*`/`DECLARE_ALIGNED`/`ff_thread_once`），目的只是让上面那两个
上游文件**一字不改**地参与编译（真正的 libavutil 头会牵进整条 FFmpeg 构建依赖）。它们适用本项目
的 Apache-2.0。

### ⚠ 分发二进制时请注意

`ebur128.c` 是 **LGPL-2.1+**。把它静态编进 `HitSound.exe` 后再分发，就等于分发了一个包含 LGPL
组件的二进制，需要按 LGPL 的要求提供该组件的源码与可重新链接的方式。若不想承担这个义务，可选：

1. 不启用响度归一化（这是可选功能，不启用时该组件完全不参与）；
2. 换成 **MIT** 许可的独立实现 [libebur128](https://github.com/jiixyj/libebur128)
   （上游 FFmpeg 那份就是从它改编的，接口几乎一致）；
3. 恢复成"动态加载外部 DLL"的旧做法（用户自备组件，动态链接不触发静态链接义务）。

### 已知的上游缺陷（本仓库不改，只记录）

`ebur128.c` 在计算通道权重时，把顶部后置声道（TBL/TBC/TBR）按 1.41 计权，而 ITU-R BS.1770 规定
高度声道一律 1.0（源于 2014 年的 `b2c0b80f`，3.4 ~ 9.0.2 全中招）。修复在 FFmpeg 的 `fc02470c`
（2026-08-06，仅进 master / 9.1-dev）。

**对本项目无影响**：本工具只做单声道打击音轨（`hit.wav` → 混音 → 单声道输出），响度测量时也只
注册一个 `FF_EBUR128_CENTER` 声道（权重 1.0），那段顶部后置声道的分支永远不会被求值；此外该缺陷
只影响含顶部后置声道的多声道布局的 M/S/I（典型 0.02~0.1 dB，纯顶部信号最多 +1.49 dB），
峰值与 LRA 不受影响。若将来要用这份代码处理多声道，请升级到含 `fc02470c` 的版本。
