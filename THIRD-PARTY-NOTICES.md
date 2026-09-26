# 第三方组件与许可证

本项目自身（`HitSound.cpp`、`Hitsound.py` 等）遵循仓库根目录的 [LICENSE](LICENSE)（Apache-2.0）。
下面这些第三方代码随仓库一起分发，各自适用**它们自己的**许可证（两者都是 MIT，不存在静态链接
传染问题）。

## 1. RapidJSON —— MIT

- 位置：`rapidjson/`
- 来源：https://github.com/Tencent/rapidjson
- 用途：解析 `.adofai` 谱面的 JSON

## 2. libebur128 —— MIT

- 位置：`third_party/ebur128/ebur128.{c,h}`（**逐字节原样，未做任何修改**）
- 来源：https://github.com/jiixyj/libebur128 ，标签 `v1.2.6`（与该仓库 master 同一提交）
  - commit：`67b33abe1558160ed76ada1322329b0e9e058b02`
  - sha256：`ebur128.c` = `c2fc562f1088cacab4d21250b6e04996ef36c7694ea901e08cc4d64eb542b78d`
  - sha256：`ebur128.h` = `a988fa03828bcdd6258e6c52300d709a58727272a9ce6cd3fbf351f090517111`
- 许可证副本：`third_party/ebur128/COPYING`（MIT）
- 用途：响度归一化的 EBU R128 测量（综合响度、响度范围、真峰值）

### 关于 `third_party/ebur128/sys/queue.h`

`ebur128.c` 里有一行 `#include <sys/queue.h>`（用于内部响度块链表），而 Windows SDK / MSVC
没有这个头。这个文件**不是** libebur128 的代码，是本项目自写的最小替身，只实现了它实际用到的
8 个 `STAILQ_*` 宏（BSD `sys/queue.h` 的经典语义），适用本项目的 Apache-2.0。编译时通过
`-Ithird_party/ebur128`（MSVC：`/Ithird_party\ebur128`）让上述 include 解析到它，
因此在三个平台上行为一致，且**上游两个文件不需要任何改动**。

### 补充说明

- 之前版本曾使用 FFmpeg 的 `libavfilter/ebur128.c`（LGPL-2.1+）。为保证 MIT 的宽松许可，
  现已换成 libebur128；后者的通道权重表只对 M±110 / M±060 / M±090 这六个声道施加 1.41 权重，
  不含"顶部后置"声道（FFmpeg 那份存在把 TBL/TBC/TBR 也按 1.41 计权的已知缺陷，见其
  commit `fc02470c`）。
- 本项目只做**单声道**测量（`hit.wav` 混单声道 → 单声道 WAV），测量时只注册一个
  `EBUR128_CENTER` 声道（权重 1.0），上述声道权重分支不会被求值。
- x86-64 上用 GCC 编译 `ebur128.c` 会打印一句 FTZ 提示（`#warning "manual FTZ is being used..."`）：
  加 `-msse2 -mfpmath=sse` 可消除；不加则走 libebur128 自带的手动 FTZ 分支，结果一致。
