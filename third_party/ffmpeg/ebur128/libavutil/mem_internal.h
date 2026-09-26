/*
 * 最小替身（shim）：ebur128.c 只从这里取 DECLARE_ALIGNED（用于把两张直方图
 * 常量表按 32 字节对齐）。定义与 FFmpeg 的 libavutil/mem_internal.h 等价。
 */
#ifndef AVUTIL_MEM_INTERNAL_H
#define AVUTIL_MEM_INTERNAL_H

#ifdef _MSC_VER
#  define DECLARE_ALIGNED(n, t, v) __declspec(align(n)) t v
#else
#  define DECLARE_ALIGNED(n, t, v) t __attribute__((aligned(n))) v
#endif

#endif /* AVUTIL_MEM_INTERNAL_H */
