#ifndef MLFQ_H
#define MLFQ_H

#include "task_small.h"

void mlfq_init(void);

void mlfq_add_task_to_queue(task_small_t* task, uint32_t queue_idx);
bool mlfq_choose_task(task_small_t** out_task, uint32_t* out_quantum);
bool mlfq_prepare_for_switch_from_task(task_small_t* task);
bool mlfq_priority_boost_if_necessary(void);
bool mlfq_next_quantum_for_task(task_small_t* task, uint32_t* out_quantum);
void mlfq_print(void);

#endif