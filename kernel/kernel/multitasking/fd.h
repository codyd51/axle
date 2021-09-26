#ifndef FILE_DESC_H
#define FILE_DESC_H

#include <stdint.h>
#include <stdbool.h>
#include "fd_entry.h"
#include <kernel/multitasking/tasks/task.h>

/* query whether file descriptor entry 'entry' refers to
 * and unused file descriptor
 */
bool fd_empty(fd_entry_t entry);

/* replace file descriptor entry at index 'index in
 * 'task's file descriptor table with an empty entry
 */
void fd_remove(task_t* task, int index);

/* add the file descriptor entry 'entry' to
 * 'task's file descriptor table at the first
 * empty location
 */
int fd_add(task_t* task, fd_entry_t entry);


/* add the file descriptor entry 'entry' to
 * 'task's file descriptor table at index 'index'
 * if an entry already exists at 'index', this silently
 * calls 'fd_remove' on it
 */
int fd_add_index(task_t* task, fd_entry_t entry, int index);

#endif
