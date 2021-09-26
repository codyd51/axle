#include "pipe.h"
#include <std/memory.h>
#include <std/std.h>
#include <kernel/util/vfs/fs.h>
#include <kernel/multitasking/tasks/task.h>
#include <kernel/multitasking/fd.h>

static void pipe_create(pipe_t** read, pipe_t** write) {
	task_t* current = task_with_pid(getpid());

	pipe_t* r = (pipe_t*)kmalloc(sizeof(pipe_t));
	memset(r, 0, sizeof(pipe_t));
	r->dir = READ;
	r->pids = array_m_create(32);
	array_m_insert(r->pids, (type_t)getpid());

	pipe_t* w = (pipe_t*)kmalloc(sizeof(pipe_t));
	memset(w, 0, sizeof(pipe_t));
	w->dir = WRITE;
	w->pids = array_m_create(32);
	array_m_insert(w->pids, (type_t)getpid());

	//add pipes to current tasks's file descriptor table
	//read pipe entry
	fd_entry_t rfd;
	rfd.type = PIPE_TYPE;
	rfd.payload = r;
	r->fd = fd_add(current, rfd);
	//write pipe entry
	fd_entry_t wfd;
	wfd.type = PIPE_TYPE;
	wfd.payload = w;
	w->fd = fd_add(current, wfd);

	//read and write pipe share the same buffer
	w->cb = kmalloc(sizeof(circular_buffer));
	cb_init(w->cb, 4096, 1);
	r->cb = w->cb;

	*read = r;
	*write = w;
}

static void pipe_destroy(pipe_t* pipe) {
	kfree(pipe);
}

int pipe(int pipefd[2]) {
	//create read and write pipes
	pipe_t* read = NULL;
	pipe_t* write = NULL;
	pipe_create(&read, &write);

	//then, assign file descriptors of new pipes to input array
	pipefd[0] = read->fd;
	pipefd[1] = write->fd;

	return 0;
}

static pipe_t* find_pipe(int fd) {
	task_t* current = task_with_pid(getpid());

	fd_entry_t entry = current->fd_table[fd];
	if (entry.type != PIPE_TYPE || fd_empty(entry)) {
		//fd passed to us was not a valid pipe!
		return NULL;
	}
	return entry.payload;
}

int pipe_read(int fd, char* buf, int count) {
	//ensure fd is a valid pipe
	pipe_t* pipe = find_pipe(fd);
	//did we find it?
	if (!pipe) {
		printf_err("pipe_read: fd %d was not valid", fd);
		return -1;
	}

	if (pipe->dir != READ) {
		printf_err("pipe_read: fd %d was not a read pipe", fd);
		return -1;
	}

	task_t* current = task_with_pid(getpid());

	int i = 0;
	for (; i < count; i++) {
		//check if we're out of items to read
		if (pipe->cb->count == 0) {
			//block until we have something to read
			block_task_context(current, PIPE_EMPTY, pipe);
			break;
		}

		cb_pop_front(pipe->cb, &(buf[i]));

		//check if we hit EOF
		/*
		if (buf[i] == EOF) {
			break;
		}
		*/
	}
	buf[i] = '\0';
	return i;
}

int pipe_write(int fd, char* buf, int count) {
	//ensure fd is a valid pipe
	pipe_t* pipe = find_pipe(fd);
	//did we find it?
	if (!pipe) {
		printf_err("pipe_write: fd %d was not valid", fd);
		return -1;
	}

	//correct direction?
	if (pipe->dir != WRITE) {
		printf_err("pipe_write: fd %d was not a write pipe", fd);
		return -1;
	}

	task_t* current = task_with_pid(getpid());

	//make sure we can fulfill the full write
	int available = pipe->cb->capacity - pipe->cb->count;
	if (available < count) {
		//block until there's more space available
		pipe_block_info info;
		info.pipe = pipe;
		info.free_bytes_needed = count;
		block_task_context(current, PIPE_FULL, &info);
		//we've unblocked, so enough space should now be available
		//we could just continue executing, but
		//recurse instead to repeat state checking
		//(as we've blocked and state may have changed)
		return pipe_write(fd, buf, count);
	}

	int i = 0;
	for (; i < count; i++) {
		ASSERT(pipe->cb->count < pipe->cb->capacity, "pipe_write() pipe %d didn't have enough free space to fulfill write!", fd);
		cb_push_back(pipe->cb, &(buf[i]));
	}
	return i;
}

int pipe_close(int fd) {
	pipe_t* pipe = find_pipe(fd);
	if (!pipe) {
		printf_err("pipe_close() fd %d was not valid", fd);
		return -1;
	}

	//if the number of tasks referencing this pipe end is exactly 1,
	//then after removing this pipe from that tasks the pipe will be
	//ready to be destroyed.
	//if this is a write pipe and this is the only task referencing it,
	//then we should write EOF
	//we need to write EOF *before* removing the pipe from the tasks's
	//list of pipes because pipe_write calls pipe_find to see if the pipe is valid,
	//which check's the process's list of pipes
	if (pipe->dir == WRITE && pipe->pids->size == 1) {
		//when closing write end,
		//write EOF to buffer
		char eof = EOF;
		pipe_write(fd, &eof, 1);
	}

	//remove current PID from list of PIDs referencing this pipe end
	int idx = array_m_index(pipe->pids, (type_t)getpid());
	if (idx == ARR_NOT_FOUND) {
		printf_err("pipe_close() on pipe not owned in proc");
		return -1;
	}
	array_m_remove(pipe->pids, idx);

	//remove this pipe from process's file descriptor list
	task_t* current = task_with_pid(getpid());
	fd_remove(current, pipe->fd);

	//if there are more processes referencing this pipe,
	//quit early
	if (pipe->pids->size) {
		return -1;
	}

	//if no PIDs are referencing this pipe end,
	//tear it down

	if (pipe->dir == READ) {
		//when closing a read end where there are no other processes
		//with a reference to the pipe, it's safe to destroy backing pipe resources
		cb_free(pipe->cb);
		kfree(pipe->cb);
	}
	pipe_destroy(pipe);

	return 0;
}
