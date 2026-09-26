/*
 * 最小 <sys/queue.h> 替身（本项目的文件，不是 libebur128 的代码）。
 *
 * ebur128.c 只用到 STAILQ_* 这一组宏，而 Windows SDK / MSVC 没有 <sys/queue.h>。
 * 这里的定义按 BSD sys/queue.h 的经典语义重写，只保留被用到的那几个宏，
 * 让 libebur128 的上游文件一字不改地参与编译。
 *
 * 使用方式：编译时加 -Ithird_party/ebur128（MSVC: /Ithird_party\ebur128），
 * 于是 ebur128.c 里的 #include <sys/queue.h> 会解析到本文件——三个平台行为一致。
 * 顺带说明：本项目只做单声道测量，这些队列仅用于内部的响度块链表，与音频无关。
 */
#ifndef HITSOUND_THIRD_PARTY_SYS_QUEUE_H
#define HITSOUND_THIRD_PARTY_SYS_QUEUE_H

#define STAILQ_HEAD(name, type)                                                \
  struct name {                                                                \
    struct type *stqh_first;                                                   \
    struct type **stqh_last;                                                   \
  }

#define STAILQ_HEAD_INITIALIZER(head) { NULL, &(head).stqh_first }

#define STAILQ_ENTRY(type)                                                     \
  struct {                                                                     \
    struct type *stqe_next;                                                    \
  }

#define STAILQ_INIT(head)                                                      \
  do {                                                                         \
    (head)->stqh_first = NULL;                                                 \
    (head)->stqh_last = &(head)->stqh_first;                                   \
  } while (0)

#define STAILQ_EMPTY(head) ((head)->stqh_first == NULL)

#define STAILQ_FIRST(head) ((head)->stqh_first)

#define STAILQ_FOREACH(var, head, field)                                       \
  for ((var) = STAILQ_FIRST(head); (var) != NULL;                              \
       (var) = (var)->field.stqe_next)

#define STAILQ_INSERT_TAIL(head, elm, field)                                   \
  do {                                                                         \
    (elm)->field.stqe_next = NULL;                                             \
    *(head)->stqh_last = (elm);                                                \
    (head)->stqh_last = &(elm)->field.stqe_next;                               \
  } while (0)

#define STAILQ_REMOVE_HEAD(head, field)                                        \
  do {                                                                         \
    if (((head)->stqh_first = (head)->stqh_first->field.stqe_next) == NULL)    \
      (head)->stqh_last = &(head)->stqh_first;                                 \
  } while (0)

#endif /* HITSOUND_THIRD_PARTY_SYS_QUEUE_H */
