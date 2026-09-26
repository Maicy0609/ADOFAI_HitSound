/*
 * 最小替身（shim）：ebur128.c 用 AVERROR(ENOMEM) / AVERROR(EINVAL) 返回错误。
 * 定义照抄 FFmpeg 的 libavutil/error.h（非 Windows 分支）：AVERROR(e) = -(e)。
 */
#ifndef AVUTIL_ERROR_H
#define AVUTIL_ERROR_H

#include <errno.h>

#define AVERROR(e) (-(e))

#endif /* AVUTIL_ERROR_H */
