/*
 * 最小替身（shim）：FFmpeg 的 libavfilter/ebur128.c 需要一个「只执行一次」的
 * 初始化原语（ff_thread_once / AVOnce / AV_ONCE_INIT），它原本来自
 * libavutil/thread.h（Linux 下是 pthread_once 的包装）。
 *
 * 这里只提供 API 形状与语义等价的最简实现：ebur128.c 只在 init 时调用一次
 * 初始化直方图常量表，且我们的用法是单线程的，因此用「标志位 + 直接调用」即可。
 * 目的仅是让 FFmpeg 的 ebur128.c **原样**参与编译，不改动那一份上游源码。
 */
#ifndef AVUTIL_THREAD_H
#define AVUTIL_THREAD_H

typedef int AVOnce;
#define AV_ONCE_INIT 0

static inline int ff_thread_once(AVOnce *once, void (*func)(void))
{
    if (!*once) {
        *once = 1;
        func();
    }
    return 0;
}

#endif /* AVUTIL_THREAD_H */
