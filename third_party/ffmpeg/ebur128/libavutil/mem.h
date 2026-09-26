/*
 * 最小替身（shim）：ebur128.c 用到的 5 个 libavutil 内存函数。
 * 语义与 FFmpeg 一致（av_mallocz/av_calloc 清零、av_malloc_array 做乘法溢出检查），
 * 唯一差别是不做 32/64 字节对齐——ebur128 只分配 double/int，16 字节对齐足够，
 * 且这里没有任何 SIMD 路径。目的同样是让上游 ebur128.c 原样编译。
 */
#ifndef AVUTIL_MEM_H
#define AVUTIL_MEM_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline void *av_malloc(size_t size)
{
    if (size == 0)
        return NULL;
    return malloc(size);
}

static inline void *av_mallocz(size_t size)
{
    void *p = av_malloc(size);
    if (p)
        memset(p, 0, size);
    return p;
}

static inline void *av_malloc_array(size_t nmemb, size_t size)
{
    if (!size || nmemb > SIZE_MAX / size)      /* 与 FFmpeg 相同的溢出检查 */
        return NULL;
    return malloc(nmemb * size);
}

static inline void *av_calloc(size_t nmemb, size_t size)
{
    if (!size)
        return NULL;
    if (nmemb > SIZE_MAX / size)
        return NULL;
    return calloc(nmemb, size);
}

static inline void av_free(void *ptr)
{
    free(ptr);
}

#endif /* AVUTIL_MEM_H */
