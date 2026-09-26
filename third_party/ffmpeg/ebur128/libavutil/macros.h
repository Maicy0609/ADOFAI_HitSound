/*
 * 最小替身（shim）：ebur128.c 只用到 FFMAX（以及 FFMIN 留作备用）。
 * 定义照抄 FFmpeg 的 libavutil/macros.h。
 */
#ifndef AVUTIL_MACROS_H
#define AVUTIL_MACROS_H

#define FFMAX(a, b) ((a) > (b) ? (a) : (b))
#define FFMIN(a, b) ((a) > (b) ? (b) : (a))
#define FFMAX3(a, b, c) FFMAX(FFMAX(a, b), c)
#define FFMIN3(a, b, c) FFMIN(FFMIN(a, b), c)

#endif /* AVUTIL_MACROS_H */
