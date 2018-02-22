/*

Voltage Logger

Copyright (C) 2018 Atle Solbakken atle@goliathdns.no

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef VL_BUFFER_H
#define VL_BUFFER_H

#include <pthread.h>
#include <string.h>
#include <stdlib.h>

struct fifo_buffer_entry {
	void *data;
	struct fifo_buffer_entry *next;
};

struct fifo_buffer {
	struct fifo_buffer_entry *gptr_first;
	struct fifo_buffer_entry *gptr_last;
	pthread_mutex_t mutex;
	int readers;
	int writers;
};

static inline void fifo_write_lock(struct fifo_buffer *buffer) {
	int ok = 0;
	while (ok != 3) {
		pthread_mutex_lock(&buffer->mutex);
		if (buffer->writers == 0) {
			ok = 1;
		}
		if (ok == 1) {
			buffer->writers = 1;
			ok = 2;
		}
		if (ok == 2) {
			if (buffer->readers == 0) {
				ok = 3;
			}
		}
		pthread_mutex_unlock(&buffer->mutex);
	}
}

static inline void fifo_write_unlock(struct fifo_buffer *buffer) {
	pthread_mutex_lock(&buffer->mutex);
	buffer->writers = 0;
	pthread_mutex_unlock(&buffer->mutex);
}

static inline void fifo_read_lock(struct fifo_buffer *buffer) {
	int ok = 0;
	while (!ok) {
		pthread_mutex_lock(&buffer->mutex);
		if (buffer->writers == 0) {
			buffer->readers++;
			ok = 1;
		}
		pthread_mutex_unlock(&buffer->mutex);
	}
}

static inline void fifo_read_unlock(struct fifo_buffer *buffer) {
	pthread_mutex_lock(&buffer->mutex);
	buffer->readers--;
	pthread_mutex_unlock(&buffer->mutex);
}

/*
 * This reading method holds a write lock for a minum amount of time by
 * taking control of the start of the queue making it inaccessible to
 * others.
 */
static inline void fifo_read_clear_forward (
		struct fifo_buffer *buffer,
		struct fifo_buffer_entry *last_element,
		void (*callback)(void *data)
) {
	fifo_write_lock(buffer);

	struct fifo_buffer_entry *current = buffer->gptr_first;
	buffer->gptr_first = last_element->next;

	fifo_write_unlock(buffer);

	struct fifo_buffer_entry *stop = last_element->next;
	while (current != stop) {
		struct fifo_buffer_entry *next = current->next;

		callback(current->data);

		free(current->data);
		free(current);

		current = next;
	}
}

/*
 * This reading method blocks writers but allow other readers to traverse at the
 * same time.
 */
static inline void fifo_read(struct fifo_buffer *buffer, void (*callback)(void *data)) {
	fifo_read_lock(buffer);

	struct fifo_buffer_entry *first = buffer->gptr_first;
	while (first != NULL) {
		callback(first->data);
		first = first->next;
	}

	fifo_read_unlock(buffer);

}
/*
 * This writing method holds the lock for a minimum amount of time, only to
 * update the pointers to the end.
 */
static inline void fifo_buffer_write(struct fifo_buffer *buffer, void *data) {
	struct fifo_buffer_entry *entry = malloc(sizeof(*entry));
	memset (entry, '\0', sizeof(*entry));
	entry->data = data;

	fifo_write_lock(buffer);

	if (buffer->gptr_last == NULL) {
		buffer->gptr_last = entry;
		buffer->gptr_first = entry;
	}
	else {
		buffer->gptr_last->next = entry;
		buffer->gptr_last = entry;
	}

	fifo_write_unlock(buffer);
}

void fifo_buffer_destroy();
struct fifo_buffer *fifo_buffer_init();

#endif
