/*

Read Route Record

Copyright (C) 2021 Atle Solbakken atle@goliathdns.no

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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>

#include <poll.h>

#include "../log.h"
#include "../allocator.h"
#include "event.h"
#include "event_struct.h"
#include "event_functions.h"
#include "../rrr_strerror.h"
#include "../rrr_config.h"
#include "../rrr_path_max.h"
#include "../string_builder.h"
#include "../socket/rrr_socket.h"
#include "../socket/rrr_socket_eventfd.h"
#include "../util/gnu.h"
#include "../util/rrr_time.h"

int rrr_event_queue_reinit (
		struct rrr_event_queue *queue
) {
	return event_reinit(queue->event_base) != 0;
}

void rrr_event_queue_fds_get (
		int fds[RRR_EVENT_QUEUE_FD_MAX],
		size_t *fds_count,
		struct rrr_event_queue *queue
) {
	size_t wpos = 0;
	for (size_t i = 0; i <= RRR_EVENT_FUNCTION_MAX; i++) {
		fds[wpos++] = RRR_SOCKET_EVENTFD_READ_FD(&queue->functions[i].eventfd);
		fds[wpos++] = RRR_SOCKET_EVENTFD_WRITE_FD(&queue->functions[i].eventfd);
	}
	*fds_count = wpos;
}

void rrr_event_function_set (
		struct rrr_event_queue *handle,
		uint8_t code,
		int (*function)(RRR_EVENT_FUNCTION_ARGS),
		const char *description
) {
	if (function == NULL) {
		RRR_BUG("BUG: Function was NULL in rrr_event_function_set\n");
	}
	RRR_DBG_9_PRINTF("EQ SETF %p %u->%p (%s)\n", handle, code, function, description);
	handle->functions[code].function = function;
}

void rrr_event_function_set_with_arg (
		struct rrr_event_queue *handle,
		uint8_t code,
		int (*function)(RRR_EVENT_FUNCTION_ARGS),
		void *arg,
		const char *description
) {
	if (function == NULL) {
		RRR_BUG("BUG: Function was NULL in rrr_event_function_set_with_arg\n");
	}
	handle->functions[code].function = function;
	handle->functions[code].function_arg = arg;
	RRR_DBG_9_PRINTF("EQ SETF %p %u->%p(%p) (%s)\n", handle, code, function, arg, description);
}

static void __rrr_event_periodic (
		evutil_socket_t fd,
		short flags,
		void *arg
) {
	struct rrr_event_queue *queue = arg;

	(void)(fd);
	(void)(flags);

	if ( queue->callback_periodic != NULL &&
	    (queue->callback_ret = queue->callback_periodic(queue->callback_arg)) != 0
	) {
		event_base_loopbreak(queue->event_base);
	}
}

static void __rrr_event_set_pause (
		struct rrr_event_queue *queue,
		int do_pause
) {
	for (size_t i = 0; i <= RRR_EVENT_FUNCTION_MAX; i++) {
		if (queue->functions[i].signal_event) {
			if (do_pause) {
				event_del(queue->functions[i].signal_event);
			}
			else {
				event_add(queue->functions[i].signal_event, NULL);
			}
		}
	}

	if ((queue->is_paused = do_pause) != 0) {
		struct timeval tv = { 0, 50 }; // 50 us
		event_add(queue->unpause_event, &tv);
	}

	RRR_DBG_9_PRINTF("EQ DISP %p pause is %i\n",
		queue, queue->is_paused);
}

static void __rrr_event_unpause (
		evutil_socket_t fd,
		short flags,
		void *arg
) {
	struct rrr_event_queue *queue = arg;

	(void)(fd);
	(void)(flags);

	__rrr_event_set_pause(queue, 0);
}

static void __rrr_event_signal_event (
	evutil_socket_t fd,
	short flags,
	void *arg
) {
 	struct rrr_event_function *function = arg;
	struct rrr_event_queue *queue = function->queue;

	(void)(fd);
	(void)(flags);

 	int ret = 0;
	uint64_t count = 0;

	if (queue->callback_pause) {
		queue->callback_pause(&queue->is_paused, queue->is_paused, queue->callback_pause_arg);
	}

	if (queue->is_paused) {
		__rrr_event_set_pause(queue, 1);
		goto out;
	}
	else {
		if ((ret = rrr_socket_eventfd_read(&count, &function->eventfd)) != 0) {
			if (ret == RRR_SOCKET_NOT_READY) {
				// OK, nothing to do
			}
			else {
				RRR_DBG_9_PRINTF("EQ DISP %p error from eventfd read, ending loop\n", queue);
				event_base_loopbreak(queue->event_base);
			}
		}
	}

	if (function->function == NULL) {
		RRR_DBG_9_PRINTF("EQ DISP %p function not registered\n", queue);
		goto out;
	}

	RRR_DBG_9_PRINTF("EQ DISP %p count %" PRIu64 " function %p\n",
		queue, count, function->function);

	while (count > 0) {
		uint16_t amount = (count > 0xffff ? 0xffff : count);
		count -= amount;

		const uint16_t amount_orig = amount;

		if ((ret = function->function (
				&amount,
				function->function_arg != NULL
					? function->function_arg
					: queue->callback_arg
		)) != 0) {
			if (ret == RRR_EVENT_EXIT) {
				RRR_DBG_9_PRINTF("EQ DISP %p exit command from callback, ending loop\n", queue);
			}
			else {
				RRR_DBG_9_PRINTF("EQ DISP %p error %i from callback, ending loop\n", queue, ret);
			}
			queue->callback_ret = ret;
			event_base_loopbreak(queue->event_base);
			goto out;
		}

		if (amount_orig == amount) {
			// This can happen if the sender incorrectly PASSes prior to data being written to the buffer
			// in question. In case of bad performance, also verify that the reader is able to handle all
			// received messages or that it activates the pausing if it's not able to.
			sched_yield();
		}

		if (amount > 0) {
			count += amount;
		}

		RRR_DBG_9_PRINTF("EQ DISP %p => count %" PRIu64 " (remaining)\n",
			queue, count);
	}

	out:
	return;
}

void rrr_event_callback_pause_set (
		struct rrr_event_queue *queue,
		void (*callback)(int *do_pause, int is_paused, void *callback_arg),
		void *callback_arg
) {
	queue->callback_pause = callback;
	queue->callback_pause_arg = callback_arg;
}

int rrr_event_dispatch_once (
		struct rrr_event_queue *queue
) {
	return (event_base_loop(queue->event_base, EVLOOP_ONCE) < 0);
}

int rrr_event_dispatch (
		struct rrr_event_queue *queue,
		unsigned int periodic_interval_us,
		int (*function_periodic)(RRR_EVENT_FUNCTION_PERIODIC_ARGS),
		void *callback_arg
) {
	int ret = 0;

	struct timeval tv_interval = {0};

	tv_interval.tv_usec = periodic_interval_us % 1000000;
	tv_interval.tv_sec = (periodic_interval_us - tv_interval.tv_usec) / 1000000;

/*	if (event_priority_set(queue->periodic_event, RRR_EVENT_PRIORITY_HIGH) != 0) {
		RRR_MSG_0("Failed to set priority of periodict event in rrr_event_dispatch\n");
		ret = 1;
		goto out;
	}*/

	if (event_add(queue->periodic_event, &tv_interval)) {
		RRR_MSG_0("Failed to add periodic event in rrr_event_dispatch\n");
		ret = 1;
		goto out;
	}

	queue->callback_periodic = function_periodic;
	queue->callback_arg = callback_arg;
	queue->callback_ret = 0;

	if ((ret = event_base_dispatch(queue->event_base)) != 0) {
		RRR_MSG_0("Error from event_base_dispatch in rrr_event_dispatch: %i\n", ret);
		ret = 1;
		goto out;
	}

	if (queue->callback_ret != 0) {
		ret = queue->callback_ret & ~(RRR_EVENT_EXIT);
	}
	else if (event_base_got_break(queue->event_base)) {
		ret = 1;
		goto out;
	}

	out:
	return ret;
}

void rrr_event_dispatch_break (
		struct rrr_event_queue *queue
) {
	event_base_loopbreak(queue->event_base);
}

void rrr_event_dispatch_exit (
		struct rrr_event_queue *queue
) {
	queue->callback_ret = RRR_EVENT_EXIT;
	event_base_loopbreak(queue->event_base);
}

void rrr_event_dispatch_restart (
		struct rrr_event_queue *queue
) {
	event_base_loopcontinue(queue->event_base);
}

int rrr_event_pass (
		struct rrr_event_queue *queue,
		uint8_t function,
		uint8_t amount,
		int (*retry_callback)(void *arg),
		void *retry_callback_arg
) {
	int ret = 0;

	if (function > RRR_EVENT_FUNCTION_MAX) {
		RRR_BUG("BUG: Function out of range in rrr_event_pass\n");
	}

	RRR_DBG_9_PRINTF("EQ PASS %p function %u amount %u\n",
		queue, function, amount);

	retry:
	if ((ret = rrr_socket_eventfd_write(&queue->functions[function].eventfd, amount)) != 0) {
		if (ret == RRR_SOCKET_NOT_READY) {
			if (retry_callback != NULL && (ret = retry_callback(retry_callback_arg) != 0)) {
				goto out;
			}
			goto retry;
		}
		RRR_MSG_0("Failed to pass event in rrr_event_pass, return was %i\n", ret);
		ret = RRR_EVENT_ERR;
		goto out;
	}

	out:
	return ret;
}

void rrr_event_queue_destroy (
		struct rrr_event_queue *queue
) {
	RRR_DBG_9_PRINTF("EQ DSTY %p\n", queue);

	for (size_t i = 0; i <= RRR_EVENT_FUNCTION_MAX; i++) {
		rrr_socket_eventfd_cleanup(&queue->functions[i].eventfd);
		if (queue->functions[i].signal_event != NULL) {
			event_free(queue->functions[i].signal_event);
		}
	}
	if (queue->periodic_event != NULL) {
		event_free(queue->periodic_event);
	}
	if (queue->unpause_event != NULL) {
		event_free(queue->unpause_event);
	}
	event_base_free(queue->event_base);
	rrr_free(queue);
}

int rrr_event_queue_new (
		struct rrr_event_queue **target
) {
	int ret = 0;

	struct event_config *cfg = NULL;

	*target = NULL;

	struct rrr_event_queue *queue = NULL;

	if ((cfg = event_config_new()) == NULL) {
		RRR_MSG_0("Could not create event config in rrr_event_queue_new\n");
		ret = 1;
		goto out;
	}

	// epoll does not work with UNIX files, use poll instead
	if (event_config_avoid_method(cfg, "epoll") != 0) {
		RRR_MSG_0("event_config_avoid_method() failed in rrr_event_queue_new\n");
		ret = 1;
		goto out;
	}

	if ((queue = rrr_allocate(sizeof(*queue))) == NULL) {
		RRR_MSG_0("Failed to allocate memory in rrr_event_queue_new\n");
		ret = 1;
		goto out;
	}

	memset(queue, '\0', sizeof(*queue));

	if ((queue->event_base = event_base_new_with_config(cfg)) == NULL) {
		RRR_MSG_0("Could not create event base in rrr_event_queue_init\n");
		ret = 1;
		goto out_free;
	}

	if (event_base_priority_init (queue->event_base, RRR_EVENT_PRIORITY_COUNT) != 0) {
		RRR_MSG_0("Failed to initialize priority queues in rrr_event_queue_new\n");
		ret = 1;
		goto out_destroy_event_base;
	}

	if ((queue->periodic_event = event_new (
			queue->event_base,
			-1,
			EV_TIMEOUT|EV_PERSIST,
			__rrr_event_periodic,
			queue
	)) == NULL) {
		RRR_MSG_0("Failed to create periodic event in rrr_event_queue_new\n");
		ret = 1;
		goto out_destroy_event_base;
	}

	if ((queue->unpause_event = event_new (
			queue->event_base,
			-1,
			EV_TIMEOUT,
			__rrr_event_unpause,
			queue
	)) == NULL) {
		RRR_MSG_0("Failed to create unpause event in rrr_event_queue_new\n");
		ret = 1;
		goto out_destroy_periodic_event;
	}

	RRR_DBG_9_PRINTF("EQ INIT %p thread ID %llu\n", queue, (long long unsigned) rrr_gettid());

	for (size_t i = 0; i <= RRR_EVENT_FUNCTION_MAX; i++) {
		queue->functions[i].queue = queue;
		if ((ret = rrr_socket_eventfd_init(&queue->functions[i].eventfd)) != 0) {
			break;
		}
		if ((queue->functions[i].signal_event = event_new (
				queue->event_base,
				RRR_SOCKET_EVENTFD_READ_FD(&queue->functions[i].eventfd),
				EV_READ | EV_PERSIST,
				__rrr_event_signal_event,
				&queue->functions[i]
		)) == NULL) {
			RRR_MSG_0("Failed to create signal event in rrr_event_queue_new\n");
			ret = 1;
			break;
		}
		if (event_add (queue->functions[i].signal_event, NULL) != 0) {
			RRR_MSG_0("Failed to add signal event in rrr_event_queue_new\n");
			ret = 1;
			break;
		}

		RRR_DBG_9_PRINTF(" -      function %llu fds %i<-%i\n",
				(long long unsigned int) i,
				RRR_SOCKET_EVENTFD_READ_FD(&queue->functions[i].eventfd),
				RRR_SOCKET_EVENTFD_WRITE_FD(&queue->functions[i].eventfd)
		);
	}

	if (ret != 0) {
		RRR_MSG_0("Failed to initialize event FDs in rrr_event_queue_new\n");
		ret = 1;
		goto out_cleanup_eventfd;
	}

	*target = queue;

	goto out;
	out_cleanup_eventfd:
		for (size_t i = 0; i <= RRR_EVENT_FUNCTION_MAX; i++) {
			rrr_socket_eventfd_cleanup(&queue->functions[i].eventfd);
			if (queue->functions[i].signal_event != NULL) {
				event_free(queue->functions[i].signal_event);
			}
		}
	out_destroy_periodic_event:
		event_free(queue->periodic_event);
	out_destroy_event_base:
		event_base_free(queue->event_base);
	out_free:
		rrr_free(queue);
	out:
		if (cfg != NULL) {
		        event_config_free(cfg);
		}
		return ret;
}
