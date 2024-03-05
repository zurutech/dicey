// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(ORNYMSAZGZ_PENDING_LIST_H)
#define ORNYMSAZGZ_PENDING_LIST_H

#include <stdbool.h>
#include <stdint.h>

#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif

struct dicey_waiting_task {
    uint32_t packet_seq;
    uint64_t task_id;
};

struct dicey_waiting_list;

bool dicey_waiting_list_append(struct dicey_waiting_list **list_ptr, uint32_t seq, uint64_t task_id);
const struct dicey_waiting_task *dicey_waiting_list_begin(const struct dicey_waiting_list *list);

void dicey_waiting_list_clear(struct dicey_waiting_list *list);

const struct dicey_waiting_task *dicey_waiting_list_end(const struct dicey_waiting_list *list);

bool dicey_waiting_list_remove_seq(struct dicey_waiting_list *list, uint32_t seq, uint64_t *task_id);

bool dicey_waiting_list_remove_task(struct dicey_waiting_list *list, uint64_t task_id, uint32_t *seq);

#endif // ORNYMSAZGZ_PENDING_LIST_H
