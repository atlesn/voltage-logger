/*

Read Route Record

Copyright (C) 2019-2021 Atle Solbakken atle@goliathdns.no

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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "../log.h"
#include "../allocator.h"
#include "udpstream_asd.h"
#include "../buffer.h"
#include "../read.h"
#include "../ip/ip.h"
#include "../message_holder/message_holder.h"
#include "../message_holder/message_holder_struct.h"
#include "../socket/rrr_socket_constants.h"
#include "../messages/msg_checksum.h"
#include "../messages/msg_msg.h"
#include "../messages/msg.h"
#include "../util/macro_utils.h"
#include "../util/rrr_time.h"
#include "../util/posix.h"

static struct rrr_udpstream_asd_control_msg __rrr_udpstream_asd_control_msg_split (uint64_t application_data) {
	struct rrr_udpstream_asd_control_msg result;

	result.flags = application_data >> 32;
	result.message_id = application_data & 0xffffffff;

	return result;
}

static uint64_t __rrr_udpstream_asd_control_msg_join (struct rrr_udpstream_asd_control_msg msg) {
	uint64_t result;

	result = (((uint64_t) msg.flags) << 32) | ((uint64_t) msg.message_id);

	return result;
}

static int __rrr_udpstream_asd_queue_entry_destroy (
		struct rrr_udpstream_asd_queue_entry *entry
) {
	if (entry->message != NULL) {
		rrr_msg_holder_decref(entry->message);
	}
	rrr_free(entry);
	return 0;
}

static struct rrr_udpstream_asd_queue_entry *__rrr_udpstream_asd_queue_find_entry (
		struct rrr_udpstream_asd_queue_new *queue,
		uint32_t message_id
) {
	if (RRR_LL_FIRST(queue) != NULL && message_id < RRR_LL_FIRST(queue)->message_id) {
		return NULL;
	}
	if (RRR_LL_LAST(queue) != NULL && message_id > RRR_LL_LAST(queue)->message_id) {
		return NULL;
	}

	RRR_LL_ITERATE_BEGIN(queue, struct rrr_udpstream_asd_queue_entry);
		if (node->message_id > message_id) {
			return NULL;
		}
		if (node->message_id == message_id) {
			return node;
		}
	RRR_LL_ITERATE_END();

	return NULL;
}

static struct rrr_udpstream_asd_queue_entry *__rrr_udpstream_asd_queue_collection_find_entry (
		struct rrr_udpstream_asd_queue_collection *collection,
		uint32_t connect_handle,
		uint32_t message_id
) {
	RRR_LL_ITERATE_BEGIN(collection, struct rrr_udpstream_asd_queue_new);
		if (node->source_connect_handle == connect_handle) {
			return __rrr_udpstream_asd_queue_find_entry(node, message_id);
		}
	RRR_LL_ITERATE_END();

	return NULL;
}

int __rrr_udpstream_asd_queue_collection_iterate (
		struct rrr_udpstream_asd_queue_collection *collection,
		int (*callback)(struct rrr_udpstream_asd_queue_new *queue, void *private_arg),
		void *private_arg
) {
	int ret = 0;

	RRR_LL_ITERATE_BEGIN(collection, struct rrr_udpstream_asd_queue_new);
		if (callback(node, private_arg) != 0) {
			ret = 1;
			goto out;
		}
	RRR_LL_ITERATE_END();

	out:
	return ret;
}

int __rrr_udpstream_asd_queue_collection_count_entries (
		struct rrr_udpstream_asd_queue_collection *collection
) {
	int total = 0;

	RRR_LL_ITERATE_BEGIN(collection, struct rrr_udpstream_asd_queue_new);
		total += RRR_LL_COUNT(node);
	RRR_LL_ITERATE_END();

	return total;
}


static void __rrr_udpstream_asd_queue_insert_ordered (
		struct rrr_udpstream_asd_queue_new *queue,
		struct rrr_udpstream_asd_queue_entry *entry
) {
	if (RRR_LL_LAST(queue) == NULL || RRR_LL_LAST(queue)->message_id < entry->message_id) {
		RRR_LL_APPEND(queue, entry);
		return;
	}

	RRR_LL_ITERATE_BEGIN(queue, struct rrr_udpstream_asd_queue_entry);
		if (entry->message_id < node->message_id) {
			RRR_LL_ITERATE_INSERT(queue, entry);
			entry = NULL;
			RRR_LL_ITERATE_BREAK();
		}
	RRR_LL_ITERATE_END();

	if (entry != NULL) {
		RRR_LL_ITERATE_BEGIN(queue, struct rrr_udpstream_asd_queue_entry);
			RRR_MSG_0("dump queue boundaries: %" PRIu32 "\n", node->message_id);
		RRR_LL_ITERATE_END();
		RRR_BUG("Entry with boundary %" PRIu32 " was not inserted in __rrr_udpstream_asd_queue_insert_ordered\n", entry->message_id);
	}
}

static int __rrr_udpstream_asd_queue_incref_and_insert_entry (
		struct rrr_udpstream_asd_queue_new *queue,
		struct rrr_msg_holder *ip_entry,
		uint32_t message_id
) {
	int ret = 0;
	struct rrr_udpstream_asd_queue_entry *new_entry = NULL;

	if (__rrr_udpstream_asd_queue_find_entry(queue, message_id) != NULL) {
		goto out;
	}

	if ((new_entry = rrr_allocate(sizeof(*new_entry))) == NULL) {
		RRR_MSG_0("Could not allocate memory in __rrr_udpstream_asd_queue_insert_entry_or_free\n");
		ret = 1;
		goto out;
	}
	memset(new_entry, '\0', sizeof(*new_entry));

	rrr_msg_holder_incref_while_locked(ip_entry);
	new_entry->message_id = message_id;
	new_entry->message = ip_entry;

	__rrr_udpstream_asd_queue_insert_ordered(queue, new_entry);
	new_entry = NULL;

	out:
	if (new_entry != NULL) {
		__rrr_udpstream_asd_queue_entry_destroy(new_entry);
	}

	return ret;
}

static int __rrr_udpstream_asd_queue_new (struct rrr_udpstream_asd_queue_new **target, uint32_t connect_handle) {
	*target = NULL;

	struct rrr_udpstream_asd_queue_new *queue = rrr_allocate(sizeof(*queue));

	if (queue == NULL) {
		RRR_MSG_0("Could not allocate memory in __rrr_udpstream_asd_queue_new\n");
		return 1;
	}

	memset(queue, '\0', sizeof(*queue));

	queue->source_connect_handle = connect_handle;

	*target = queue;

	return 0;
}

static int __rrr_udpstream_asd_queue_collection_incref_and_insert_entry (
		struct rrr_udpstream_asd_queue_collection *collection,
		struct rrr_msg_holder *entry,
		uint32_t connect_handle,
		uint32_t message_id
) {
	int ret = 0;

	struct rrr_udpstream_asd_queue_new *target = NULL;

	RRR_LL_ITERATE_BEGIN(collection, struct rrr_udpstream_asd_queue_new);
		if (connect_handle == node->source_connect_handle) {
			target = node;
			RRR_LL_ITERATE_BREAK();
		}
	RRR_LL_ITERATE_END();

	if (target == NULL) {
		if (__rrr_udpstream_asd_queue_new(&target, connect_handle) != 0) {
			RRR_MSG_0("Could not create new queue in __rrr_udpstream_asd_queue_collection_insert_entry\n");
			ret = 1;
			goto out;
		}
		RRR_LL_APPEND(collection, target);
	}

	ret = __rrr_udpstream_asd_queue_incref_and_insert_entry(target, entry, message_id);

	out:
	return ret;
}

void __rrr_udpstream_asd_queue_clear(struct rrr_udpstream_asd_queue_new *queue) {
	RRR_LL_DESTROY(queue, struct rrr_udpstream_asd_queue_entry, __rrr_udpstream_asd_queue_entry_destroy(node));
}

void __rrr_udpstream_asd_queue_destroy(struct rrr_udpstream_asd_queue_new *queue) {
	__rrr_udpstream_asd_queue_clear(queue);
	rrr_free(queue);
}

void __rrr_udpstream_asd_queue_collection_clear(struct rrr_udpstream_asd_queue_collection *collection) {
	RRR_LL_DESTROY(collection, struct rrr_udpstream_asd_queue_new, __rrr_udpstream_asd_queue_destroy(node));
}

void __rrr_udpstream_asd_control_queue_clear(struct rrr_udpstream_asd_control_queue *queue) {
	RRR_LL_DESTROY(queue, struct rrr_udpstream_asd_control_queue_entry, rrr_free(node));
}

void rrr_udpstream_asd_destroy (
		struct rrr_udpstream_asd *session
) {
	pthread_mutex_destroy(&session->queue_lock);
	pthread_mutex_destroy(&session->connect_lock);
	pthread_mutex_destroy(&session->message_id_lock);
	__rrr_udpstream_asd_queue_collection_clear(&session->release_queues);
	__rrr_udpstream_asd_queue_clear(&session->send_queue);
	__rrr_udpstream_asd_control_queue_clear(&session->control_send_queue);
	rrr_udpstream_close(&session->udpstream);
	rrr_udpstream_clear(&session->udpstream);
	RRR_FREE_IF_NOT_NULL(session->remote_host);
	RRR_FREE_IF_NOT_NULL(session->remote_port);
	rrr_free(session);
}

int rrr_udpstream_asd_new (
		struct rrr_udpstream_asd **target,
		unsigned int local_port,
		const char *remote_host,
		const char *remote_port,
		uint32_t client_id,
		int accept_connections,
		int disallow_ip_swap,
		int v4_only
) {
	int ret = 0;

	*target = NULL;

	struct rrr_udpstream_asd *session = rrr_allocate(sizeof(*session));
	if (session == NULL) {
		RRR_MSG_0("Could not allocate memory in rrr_udpstream_asd_new\n");
		ret = 1;
		goto out;
	}
	memset(session, '\0', sizeof(*session));

	if (remote_host != NULL && *remote_host != '\0') {
		if ((session->remote_host = rrr_strdup(remote_host)) == NULL) {
			RRR_MSG_0("Could not allocate remote host string in rrr_udpstream_asd_new\n");
			ret = 1;
			goto out_free;
		}
	}

	if (remote_port != NULL && *remote_port != '\0') {
		if ((session->remote_port = rrr_strdup(remote_port)) == NULL) {
			RRR_MSG_0("Could not allocate remote port string in rrr_udpstream_asd_new\n");
			ret = 1;
			goto out_free_remote_host;
		}
	}

	int udpstream_flags = 0;
	if (accept_connections != 0) {
		udpstream_flags |= RRR_UDPSTREAM_FLAGS_ACCEPT_CONNECTIONS;
	}
	if (disallow_ip_swap != 0) {
		udpstream_flags |= RRR_UDPSTREAM_FLAGS_DISALLOW_IP_SWAP;
	}

	if ((ret = rrr_udpstream_init (&session->udpstream, udpstream_flags|RRR_UDPSTREAM_FLAGS_FIXED_CONNECT_HANDLE)) != 0) {
		RRR_MSG_0("Could not initialize udpstream in rrr_udpstream_asd_new\n");
		goto out_free_remote_port;
	}

	if (v4_only) {
		if ((ret = rrr_udpstream_bind_v4_only(&session->udpstream, local_port)) != 0) {
			RRR_MSG_0("Could not bind to local port %u with IPv4 only in rrr_udpstream_asd_new\n", local_port);
			goto out_clear_udpstream;
		}
	}
	else {
		if ((ret = rrr_udpstream_bind_v6_priority(&session->udpstream, local_port)) != 0) {
			RRR_MSG_0("Could not bind to local port %u with IPv6 priority in rrr_udpstream_asd_new\n", local_port);
			goto out_clear_udpstream;
		}
	}

	if (rrr_posix_mutex_init(&session->message_id_lock, 0) != 0) {
		RRR_MSG_0("Could not initialize id lock in rrr_udpstream_asd_new\n");
		ret = 1;
		goto out_close_udpstream;
	}

	if (rrr_posix_mutex_init(&session->connect_lock, 0) != 0) {
		RRR_MSG_0("Could not initialize connect lock in rrr_udpstream_asd_new\n");
		ret = 1;
		goto out_destroy_id_lock;
	}

	if (rrr_posix_mutex_init(&session->queue_lock, 0) != 0) {
		RRR_MSG_0("Could not initialize queue lock in rrr_udpstream_asd_new\n");
		ret = 1;
		goto out_destroy_connect_lock;
	}

	session->connect_handle = client_id;

	*target = session;
	session = NULL;
	goto out;

	out_destroy_connect_lock:
		pthread_mutex_destroy(&session->connect_lock);
	out_destroy_id_lock:
		pthread_mutex_destroy(&session->message_id_lock);
	out_close_udpstream:
		rrr_udpstream_close(&session->udpstream);
	out_clear_udpstream:
		rrr_udpstream_clear(&session->udpstream);
	out_free_remote_port:
		rrr_free(session->remote_port);
	out_free_remote_host:
		rrr_free(session->remote_host);
	out_free:
		rrr_free(session);
	out:
		return ret;
}

static int __rrr_udpstream_asd_queue_control_frame (
		struct rrr_udpstream_asd *session,
		uint32_t connect_handle,
		uint32_t message_id,
		uint32_t ack_flags
) {
	int ret = 0;

	struct rrr_udpstream_asd_control_queue_entry *entry = rrr_allocate(sizeof(*entry));
	if (entry == NULL) {
		RRR_MSG_0("Could not allocate memory in __rrr_udpstream_asd_queue_control_frame\n");
		ret = 1;
		goto out;
	}

	entry->connect_handle = connect_handle;
	entry->ack_flags = ack_flags;
	entry->message_id = message_id;

	RRR_LL_APPEND(&session->control_send_queue, entry);

	out:
	return ret;
}

int __rrr_udpstream_asd_send_control_message (
		struct rrr_udpstream_asd *session,
		uint32_t flags,
		uint32_t connect_handle,
		uint32_t message_id
) {
	struct rrr_udpstream_asd_control_msg control_msg = {
			flags,
			message_id
	};

	uint64_t application_data = __rrr_udpstream_asd_control_msg_join(control_msg);

	RRR_DBG_3("ASD TX %" PRIu32 ":%" PRIu32 " CTRL flags %" PRIu32 "\n",
			connect_handle, message_id, flags);

	return rrr_udpstream_send_control_frame(&session->udpstream, connect_handle, application_data);
}

static int __rrr_udpstream_asd_buffer_connect_if_needed (
		struct rrr_udpstream_asd *session
) {
	int ret = RRR_UDPSTREAM_ASD_OK;

	pthread_mutex_lock(&session->connect_lock);

	int udpstream_ret = rrr_udpstream_connection_check(&session->udpstream, session->connect_handle);
	if (udpstream_ret == 0) {
		session->connection_attempt_time = 0;
		if (session->is_connected == 0) {
			RRR_DBG_3("ASD %" PRIu32 " ready\n", session->connect_handle);
			RRR_DBG_3("ASD TX %" PRIu32 " RST\n", session->connect_handle);
			if ((ret = __rrr_udpstream_asd_send_control_message(session, RRR_UDPSTREAM_ASD_ACK_FLAGS_RST, session->connect_handle, 0)) != 0) {
				RRR_MSG_0("Could not queue reset frame in __rrr_udpstream_asd_buffer_connect_if_needed\n");
				goto out;
			}
			session->is_connected = 1;
		}
		goto out;
	}
	else if (udpstream_ret == RRR_UDPSTREAM_NOT_READY) {
		session->is_connected = 0;
	}
	else {
		session->is_connected = 0;
	}

	if (session->remote_host != NULL && *(session->remote_host) != '\0') {
		if (session->connection_attempt_time > 0) {
			if (rrr_time_get_64() - session->connection_attempt_time > RRR_UDPSTREAM_ASD_CONNECT_TIMEOUT_MS * 1000) {
				RRR_MSG_0("Connection attempt to remote %s:%s timed out after %i ms in UDP-stream ASD session\n",
						session->remote_host, session->remote_port, RRR_UDPSTREAM_ASD_CONNECT_TIMEOUT_MS);
				session->connection_attempt_time = 0;
			}
			else {
				goto out;
			}
		}

		uint32_t connect_handle = session->connect_handle;

		RRR_DBG_3("ASD %u CONNECT send queue count %i\n",
				session->connect_handle, RRR_LL_COUNT(&session->send_queue));

		if ((ret = rrr_udpstream_connect (
				&connect_handle,
				&session->udpstream,
				session->remote_host,
				session->remote_port
		)) != 0) {
			RRR_MSG_0("Could not send connect to remote %s:%s in __rrr_udpstream_asd_buffer_connect_if_needed\n",
					session->remote_host, session->remote_port);
			ret = 1;
			goto out;
		}

		if (connect_handle != session->connect_handle) {
			RRR_BUG("Connect handle was changed by UDP-stream in __rrr_udpstream_asd_buffer_connect_if_needed\n");
		}

		session->connection_attempt_time = rrr_time_get_64();
	}

	out:
	if (ret == 0 && !session->is_connected) {
		ret = RRR_UDPSTREAM_ASD_NOT_READY;
	}
	pthread_mutex_unlock(&session->connect_lock);
	return ret;
}

void __rrr_udpstream_asd_release_queue_clear_by_handle (
		struct rrr_udpstream_asd *session,
		uint32_t connect_handle
) {
	RRR_LL_ITERATE_BEGIN(&session->release_queues, struct rrr_udpstream_asd_queue_new);
		if (node->source_connect_handle == connect_handle) {
			__rrr_udpstream_asd_queue_clear(node);
		}
	RRR_LL_ITERATE_END();
}

static int __rrr_udpstream_asd_control_frame_listener (
		uint32_t connect_handle,
		uint64_t application_data,
		void *arg
) {
	int ret = 0;

	struct rrr_udpstream_asd *session = arg;

	pthread_mutex_lock(&session->queue_lock);

	struct rrr_udpstream_asd_control_msg control_msg = __rrr_udpstream_asd_control_msg_split(application_data);

	struct rrr_udpstream_asd_queue_entry *node = NULL;

	uint32_t reply_ack_flags = 0;

	if (control_msg.flags == RRR_UDPSTREAM_ASD_ACK_FLAGS_DACK) {
		RRR_DBG_3("ASD RX %" PRIu32 ":%" PRIu32 " DACK\n",
				session->connect_handle, control_msg.message_id);

		node = __rrr_udpstream_asd_queue_find_entry(
				&session->send_queue,
				control_msg.message_id
		);
		if (node != NULL) {
			node->ack_status_flags |= RRR_UDPSTREAM_ASD_ACK_FLAGS_DACK;
		}

		RRR_DBG_3("ASD TX %" PRIu32 ":%" PRIu32 " RACK\n",
				session->connect_handle, control_msg.message_id);
		reply_ack_flags = RRR_UDPSTREAM_ASD_ACK_FLAGS_RACK;
	}
	else if (control_msg.flags == RRR_UDPSTREAM_ASD_ACK_FLAGS_RACK) {
		node = __rrr_udpstream_asd_queue_collection_find_entry(
				&session->release_queues,
				connect_handle,
				control_msg.message_id
		);

		if (node != NULL) {
			RRR_DBG_3("ASD RX %" PRIu32 ":%" PRIu32 " RACK\n",
					session->connect_handle, control_msg.message_id);
			node->ack_status_flags |= RRR_UDPSTREAM_ASD_ACK_FLAGS_RACK;
		}
		else {
			RRR_DBG_3("ASD RX %" PRIu32 ":%" PRIu32 " RACK (unknown)\n",
					session->connect_handle, control_msg.message_id);
		}

		RRR_DBG_3("ASD TX %" PRIu32 ":%" PRIu32 " CACK\n",
				session->connect_handle, control_msg.message_id);
		reply_ack_flags = RRR_UDPSTREAM_ASD_ACK_FLAGS_CACK;
	}
	else if (control_msg.flags == RRR_UDPSTREAM_ASD_ACK_FLAGS_CACK) {
		RRR_DBG_3("ASD RX %" PRIu32 ":%" PRIu32 " CACK\n",
				session->connect_handle, control_msg.message_id);

		node = __rrr_udpstream_asd_queue_find_entry (
				&session->send_queue,
				control_msg.message_id
		);
		if (node != NULL) {
			node->ack_status_flags |= RRR_UDPSTREAM_ASD_ACK_FLAGS_CACK;
		}
	}
	else if (control_msg.flags == RRR_UDPSTREAM_ASD_ACK_FLAGS_RST) {
		RRR_DBG_3("ASD RX %" PRIu32 " RST\n",
				connect_handle);
		__rrr_udpstream_asd_release_queue_clear_by_handle(session, connect_handle);
	}
	else {
		RRR_DBG_3("ASD RX %" PRIu32 " ACK (unknown flags %" PRIu32 ")\n",
				control_msg.message_id, control_msg.flags);
	}

	// We cannot reply with ACK messages immediately as we already are in locked UDP-stream
	// context. Instead, the control messages to be sent are queued and sent in the next send iteration.
	// Corresponding ACKs to received ACKs are always sent, also when the IDs are not found in the
	// buffers.
	if (reply_ack_flags != 0) {
		ret = __rrr_udpstream_asd_queue_control_frame(
				session,
				connect_handle,
				control_msg.message_id,
				reply_ack_flags
		);
	}

	pthread_mutex_unlock(&session->queue_lock);
	return ret;
}

int rrr_udpstream_asd_queue_and_incref_message (
		struct rrr_udpstream_asd *session,
		struct rrr_msg_holder *ip_message
) {
	int ret = RRR_UDPSTREAM_ASD_OK;
	uint32_t id = 0;

	if (session->remote_host == NULL || *(session->remote_host) == '\0') {
		RRR_BUG("Attempted to queue message with rrr_udpstream_asd_queue_message while remote host was not set\n");
	}

	pthread_mutex_lock(&session->queue_lock);
	pthread_mutex_lock(&session->message_id_lock);

	if (RRR_LL_COUNT(&session->send_queue) >= RRR_UDPSTREAM_ASD_BUFFER_MAX) {
		ret = RRR_UDPSTREAM_ASD_NOT_READY;
		goto out;
	}

	id = ++(session->message_id_pos);
	if (id == 0) {
		id = ++(session->message_id_pos);
	}

	RRR_DBG_3("ASD Q %" PRIu32 ":%" PRIu32 "\n",
		session->connect_handle, session->message_id_pos);

	// Not very likely
	if (__rrr_udpstream_asd_queue_find_entry(&session->send_queue, id) != NULL) {
		RRR_BUG("IDs exhausted in rrr_udpstream_asd_queue_message\n");
	}

	if ((ret = __rrr_udpstream_asd_queue_incref_and_insert_entry (
			&session->send_queue,
			ip_message,
			id
	)) != 0) {
		RRR_MSG_0("Could not insert ASD node into send queue in rrr_udpstream_asd_queue_message\n");
		ret = 1;
		goto out;
	}

	out:
	pthread_mutex_unlock(&session->message_id_lock);
	pthread_mutex_unlock(&session->queue_lock);
	return ret;
}

int __rrr_udpstream_asd_send_message (
		struct rrr_udpstream_asd *session,
		struct rrr_udpstream_asd_queue_entry *node
) {
	int ret = 0;

	struct rrr_msg_msg *message = node->message->message;
	struct rrr_msg_msg *message_network = NULL;
	message_network = rrr_msg_msg_duplicate(message);
	ssize_t message_network_size = MSG_TOTAL_SIZE(message_network);

	rrr_msg_msg_prepare_for_network((struct rrr_msg_msg *) message_network);
	rrr_msg_checksum_and_to_network_endian ((struct rrr_msg *) message_network);

	// Note: There is no locking on the connect handle. If it for some reason is invalid,
	// udpstream will detect this.
	if ((ret = rrr_udpstream_queue_outbound_data (
			&session->udpstream,
			session->connect_handle,
			message_network,
			message_network_size,
			node->message_id
	)) != 0) {
		if (ret == RRR_UDPSTREAM_NOT_READY) {
			ret = RRR_UDPSTREAM_ASD_NOT_READY;
			goto out;
		}
		else if (ret == RRR_UDPSTREAM_NOT_READY || ret == RRR_UDPSTREAM_SOFT_ERR) {
			ret = RRR_UDPSTREAM_ASD_NOT_READY;
			goto out;
		}
		else {
			RRR_MSG_0("Error while queuing message for sending in UDP-stream ASD handle %u\n",
					session->connect_handle);
			ret = RRR_UDPSTREAM_ASD_HARD_ERR;
			goto out;
		}
	}

	out:
	RRR_FREE_IF_NOT_NULL(message_network);
	return ret;
}

static int __rrr_udpstream_asd_do_release_queue_send_tasks (
		struct rrr_udpstream_asd_queue_new *queue,
		void *private_arg
) {
	struct rrr_udpstream_asd *session = private_arg;

	uint64_t time_now = rrr_time_get_64();
	int ret = 0;

	RRR_LL_ITERATE_BEGIN(queue, struct rrr_udpstream_asd_queue_entry);
		if (node->send_time == 0 || time_now - node->send_time > RRR_UDPSTREAM_ASD_RESEND_INTERVAL_MS * 1000) {
			// Always update send time to prevent hardcore looping upon error conditions
			node->send_time = time_now;

			if ((node->ack_status_flags & RRR_UDPSTREAM_ASD_ACK_FLAGS_DACK) == 0 || (node->ack_status_flags & RRR_UDPSTREAM_ASD_ACK_FLAGS_RACK) == 0) {
				// We have not sent delivery ACK or need to re-send it
				RRR_DBG_3("ASD TX %" PRIu32 ":%" PRIu32 " DACK DUP\n",
						session->connect_handle, node->message_id);
				ret = __rrr_udpstream_asd_send_control_message (
						session,
						RRR_UDPSTREAM_ASD_ACK_FLAGS_DACK,
						queue->source_connect_handle,
						node->message_id
				);
				node->ack_status_flags |= RRR_UDPSTREAM_ASD_ACK_FLAGS_DACK;
				node->send_count++;
			}

			if (ret != 0) {
				RRR_DBG_3("Error while sending message B in rrr_udpstream_asd_do_send_tasks return was %i\n", ret);
				goto out;
			}
		}
	RRR_LL_ITERATE_END_CHECK_DESTROY(queue, __rrr_udpstream_asd_queue_entry_destroy(node));

	out:
	return ret;
}

static int __rrr_udpstream_asd_do_send_tasks (struct rrr_udpstream_asd *session) {
	int ret = 0;

	uint64_t time_now = rrr_time_get_64();

	pthread_mutex_lock(&session->queue_lock);

	// Send control messages queued in control message callback
	RRR_LL_ITERATE_BEGIN(&session->control_send_queue, struct rrr_udpstream_asd_control_queue_entry);
		ret = __rrr_udpstream_asd_send_control_message (
				session,
				node->ack_flags,
				node->connect_handle,
				node->message_id
		);
		if (ret == RRR_UDPSTREAM_NOT_READY || ret == RRR_UDPSTREAM_SOFT_ERR) {
			RRR_DBG_3("ASD control message deleted after failed send attempt (connect handle %u)\n", node->connect_handle);
		}
		else if (ret != 0) {
			RRR_DBG_3("Could not send ASD control message in rrr_udpstream_asd_do_send_tasks return was %i\n", ret);
			ret = 1;
			goto out;
		}
		RRR_LL_ITERATE_SET_DESTROY();
	RRR_LL_ITERATE_END_CHECK_DESTROY(&session->control_send_queue, 0; rrr_free(node));

	int buffer_was_full = 0;

	// Send data messages and reminder ACKs for outbound messages
	RRR_LL_ITERATE_BEGIN(&session->send_queue, struct rrr_udpstream_asd_queue_entry);
		if ((node->ack_status_flags & RRR_UDPSTREAM_ASD_ACK_FLAGS_CACK) != 0) {
			RRR_LL_ITERATE_SET_DESTROY();
		}
		else if (node->send_time == 0 || time_now - node->send_time > RRR_UDPSTREAM_ASD_RESEND_INTERVAL_MS * 1000) {
			// Always update send time to prevent hardcore looping upon error conditions
			node->send_time = time_now;
			node->send_count++;

			const char *dup = node->send_count > 1 ? " DUP" : "";

			if (node->ack_status_flags == 0 || node->ack_status_flags == RRR_UDPSTREAM_ASD_ACK_FLAGS_MSG) {
				// We are missing delivery ACK, re-send message
				if (buffer_was_full == 0) {
					RRR_DBG_3("ASD TX %" PRIu32 ":%" PRIu32 " MSG%s\n",
						session->connect_handle, node->message_id, dup);
					ret = __rrr_udpstream_asd_send_message(session, node);
					if (ret == RRR_UDPSTREAM_ASD_NOT_READY) {
						RRR_DBG_3("Buffer full while sending message in rrr_udpstream_asd_do_send_tasks\n");
						buffer_was_full = 1;
						ret = 0;
					}
					else {
						node->ack_status_flags |= RRR_UDPSTREAM_ASD_ACK_FLAGS_MSG;
					}
				}
			}
			else if ((node->ack_status_flags & RRR_UDPSTREAM_ASD_ACK_FLAGS_DACK) != 0) {
				RRR_DBG_3("ASD TX %" PRIu32 ":%" PRIu32 " RACK%s\n",
						session->connect_handle, node->message_id, dup);

				// We are missing complete ACK, re-send release ACK
				ret = __rrr_udpstream_asd_send_control_message(
						session,
						RRR_UDPSTREAM_ASD_ACK_FLAGS_RACK,
						session->connect_handle,
						node->message_id
				);
				node->ack_status_flags |= RRR_UDPSTREAM_ASD_ACK_FLAGS_RACK;
			}
			else {
				RRR_BUG("Unknown ACK flags %u for node in rrr_udpstream_asd_do_send_tasks\n", node->ack_status_flags);
			}

			if (ret != 0) {
				RRR_DBG_3("Error while sending message A in rrr_udpstream_asd_do_send_tasks return was %i\n", ret);
				goto out;
			}
		}
	RRR_LL_ITERATE_END_CHECK_DESTROY(&session->send_queue, __rrr_udpstream_asd_queue_entry_destroy(node));

	// Send data messages and reminder ACKs for inbound messages
	if ((ret = __rrr_udpstream_asd_queue_collection_iterate (
			&session->release_queues,
			__rrr_udpstream_asd_do_release_queue_send_tasks,
			session
	)) != 0) {
		RRR_MSG_0("Error while iterating release queues in _rrr_udpstream_asd_do_send_tasks\n");
		goto out;
	}

	out:
	pthread_mutex_unlock(&session->queue_lock);
	return ret;
}

struct rrr_asd_receive_messages_callback_data {
	struct rrr_udpstream_asd *session;
	const struct rrr_udpstream_receive_data *udpstream_receive_data;
	int count;
};

/* Disabled, not currently used. ipclient has it's own allocator.
int rrr_udpstream_asd_default_allocator (
		uint32_t size,
		int (*callback)(void **joined_data, void *allocation_handle, void *udpstream_callback_arg),
		void *udpstream_callback_arg,
		void *arg
) {
	(void)(arg);

	int ret = 0;

	struct rrr_msg_msg_holder *new_entry = NULL;

	if (rrr_msg_msg_holder_new (
			&new_entry,
			0,
			NULL,
			0,
			RRR_IP_UDP,
			NULL
	) != 0) {
		RRR_MSG_0("Could not create ip buffer message in rrr_udpstream_asd_default_allocator\n");
		ret = 1;
		goto out;
	}

	void *joined_data = NULL;

	if ((joined_data = malloc(size)) == NULL) {
		RRR_MSG_0("Could not allocate memory for joined data in __rrr_udpstream_process_receive_buffer\n");
		ret = 1;
		goto out_destroy;
	}

	rrr_msg_msg_holder_lock(new_entry);

	new_entry->message = joined_data;
	new_entry->data_length = size;

	ret = callback(&joined_data, new_entry, udpstream_callback_arg);

	if (joined_data != NULL) {
		rrr_msg_msg_holder_destroy_while_locked(new_entry);
	}
	else {
		rrr_msg_msg_holder_unlock(new_entry);
	}

	goto out;
	out_destroy:
		rrr_msg_msg_holder_destroy(new_entry);
	out:
		return ret;
}
*/

static int __rrr_udpstream_asd_receive_messages_callback_final (struct rrr_msg_msg **message, void *arg1, void *arg2) {
	(void)(arg2);

	int ret = 0;

	struct rrr_asd_receive_messages_callback_data *receive_data = arg1;
	struct rrr_udpstream_asd *session = receive_data->session;
	struct rrr_msg_holder *entry = receive_data->udpstream_receive_data->allocation_handle;

	// Any allocator must put an ip buffer entry in the allocation handle
	// The entry must be locked already at this location, allocator is responsible for ensuring that
	// The allocator must unlock the entry after the callback chain is complete

	if (receive_data->udpstream_receive_data->application_data > 0xffffffff) {
		RRR_MSG_0("Application data/message ID out of range (%" PRIu64 ") in __rrr_udpstream_asd_receive_messages_callback_final connect handle %" PRIu32 ", message dropped\n",
				receive_data->udpstream_receive_data->application_data,
				session->connect_handle
		);
		ret = 1;
		goto out;
	}

	RRR_DBG_3("ASD RX %" PRIu32 ":%" PRIu64 " MSG timestamp %" PRIu64 "\n",
			session->connect_handle, receive_data->udpstream_receive_data->application_data, (*message)->timestamp);

	if ((ret = __rrr_udpstream_asd_queue_collection_incref_and_insert_entry (
			&session->release_queues,
			entry,
			receive_data->udpstream_receive_data->connect_handle,
			receive_data->udpstream_receive_data->application_data
	)) != 0) {
		RRR_MSG_0("Could not insert ASD message into release queue\n");
		ret = 1;
		goto out;
	}
	else {
		// Tells the allocator that we are now using the memory
		*message = NULL;
	}

	receive_data->count++;

	out:
	return ret;
}

static int __rrr_udpstream_asd_receive_messages_callback (
		RRR_UDPSTREAM_FINAL_RECEIVE_CALLBACK_ARGS
) {
	struct rrr_asd_receive_messages_callback_data *callback_data = arg;

	int ret = 0;

	callback_data->udpstream_receive_data = receive_data;

#if SSIZE_MAX > RRR_LENGTH_MAX
	if ((rrr_slength) receive_data->data_size > (rrr_slength) RRR_LENGTH_MAX) {
		RRR_MSG_0("Received message too big in __rrr_udpstream_asd_receive_messages_callback\n");
		ret = RRR_UDPSTREAM_ASD_HARD_ERR;
		goto out;
	}
#endif

	if ((ret = rrr_msg_to_host_and_verify_with_callback (
			(struct rrr_msg **) joined_data,
			receive_data->data_size,
			__rrr_udpstream_asd_receive_messages_callback_final,
			NULL,
			NULL,
			NULL,
			NULL,
			callback_data,
			NULL
	)) != 0) {
		if (ret == RRR_SOCKET_SOFT_ERROR) {
			RRR_MSG_0("Invalid message received in __rrr_udpstream_asd_receive_messages_callback, application data was %" PRIu64 "\n",
					receive_data->application_data);
			ret = 0;
		}
		else {
			RRR_MSG_0("Error while processing message in __rrr_udpstream_asd_receive_messages_callback return was %i\n",
					ret);
			ret = 1;
			goto out;
		}
	}

	out:
	return ret;
}

static int __rrr_udpstream_asd_do_receive_tasks (
		int *receive_count,
		struct rrr_udpstream_asd *session,
		int (*allocator_callback)(RRR_UDPSTREAM_ALLOCATOR_CALLBACK_ARGS),
		void *allocator_callback_arg
) {
	int ret = 0;

	struct rrr_asd_receive_messages_callback_data receive_callback_data = {
			session, NULL, 0
	};

	if (__rrr_udpstream_asd_queue_collection_count_entries(&session->release_queues) < RRR_UDPSTREAM_ASD_RELEASE_QUEUE_MAX) {
		if ((ret = rrr_udpstream_do_process_receive_buffers (
				&session->udpstream,
				allocator_callback,
				allocator_callback_arg,
				rrr_read_common_get_session_target_length_from_message_and_checksum_raw,
				NULL,
				__rrr_udpstream_asd_receive_messages_callback,
				&receive_callback_data
		)) != 0) {
			RRR_MSG_0("Error from UDP-stream while processing buffers in receive_packets of UDP-stream ASD handle %u\n",
					session->connect_handle);
			ret = 1;
			goto out;
		}
	}
	else {
		RRR_DBG_3("UDP-stream ASD handle %u release queue is full\n", session->connect_handle);
	}

	*receive_count = receive_callback_data.count;

	out:
	return ret;
}

int __rrr_udpstream_asd_queue_deliver_messages (
		int *delivered_count,
		struct rrr_udpstream_asd *session,
		struct rrr_udpstream_asd_queue_new *queue,
		int (*receive_callback)(struct rrr_msg_holder *message, void *arg),
		void *receive_callback_arg
) {
	*delivered_count = 0;

	int ret = 0;

	RRR_LL_ITERATE_BEGIN(queue, struct rrr_udpstream_asd_queue_entry);
		if ((node->ack_status_flags & RRR_UDPSTREAM_ASD_ACK_FLAGS_RACK) != 0 && node->delivered_grace_counter == 0) {
			if ((node->ack_status_flags & RRR_UDPSTREAM_ASD_ACK_FLAGS_DACK) == 0) {
				RRR_BUG("RACK without DACK in __rrr_udpstream_asd_deliver_messages_from_queue\n");
			}

			struct rrr_msg_holder *message = node->message;

			RRR_DBG_3("ASD D %u:%u MSG timestamp %" PRIu64 ", grace started\n",
					session->connect_handle, node->message_id, node->send_time);

			// Callback must ALWAYS unlock
			rrr_msg_holder_lock(message);
			if ((ret = receive_callback(message, receive_callback_arg)) != 0) {
				RRR_MSG_0("Error from callback in __rrr_udpstream_asd_deliver_messages_from_queue\n");
				ret = 1;
				goto out;
			}

			rrr_msg_holder_decref(message);
			node->message = NULL;

			(*delivered_count)++;

			node->delivered_grace_counter = RRR_UDPSTREAM_ASD_DELIVERY_GRACE_COUNTER;
		}
	RRR_LL_ITERATE_END();

	out:
	return ret;
}

int __rrr_udpstream_asd_queue_update_delivery_grace (
		int *grace_count,
		struct rrr_udpstream_asd_queue_new *queue,
		int delivered_count
) {
	*grace_count = 0;

	// Once grace counter reaches zero, the queue entry is finally removed. A fixed number
	// of new messages need to be delivered after an delivered entry is removed, this is
	// to prevent ID collisions. The grace "distance" must be much greater than window size.
	RRR_LL_ITERATE_BEGIN(queue, struct rrr_udpstream_asd_queue_entry);
		if (node->delivered_grace_counter > 0) {
			(*grace_count)++;
			node->delivered_grace_counter -= delivered_count;
			// Important to check for less than or equal to zero, or the
			// entry might be delivered to application again
			if (node->delivered_grace_counter <= 0) {
				RRR_LL_ITERATE_SET_DESTROY();
			}
		}
	RRR_LL_ITERATE_END_CHECK_DESTROY(queue, __rrr_udpstream_asd_queue_entry_destroy(node));

	return 0;
}

int __rrr_udpstream_asd_queue_regulate_window_size (
		struct rrr_udpstream_asd *session,
		struct rrr_udpstream_asd_queue_new *queue,
		int grace_count
) {
	int ret = 0;

	if (RRR_LL_COUNT(queue) - grace_count > RRR_UDPSTREAM_ASD_RELEASE_QUEUE_WINDOW_SIZE_REDUCTION_THRESHOLD) {
		if ((ret = rrr_udpstream_regulate_window_size (
				&session->udpstream,
				queue->source_connect_handle,
				RRR_UDPSTREAM_ASD_WINDOW_SIZE_REDUCTION_AMOUNT
		)) != 0) {
			// Don't produce fatal error here, let something else fail
			RRR_DBG_3("Error while regulating window size in ASD while delivering messages, return from UDP-stream was %i\n", ret);
			ret = 0;
		}
	}

	return ret;
}

static int __rrr_udpstream_asd_deliver_and_maintain_queue (
		struct rrr_udpstream_asd *session,
		int (*receive_callback)(struct rrr_msg_holder *message, void *arg),
		void *receive_callback_arg,
		struct rrr_udpstream_asd_queue_new *queue
) {
	int ret = 0;

	int graced_messages_count = 0;
	int delivered_messages_count = 0;

	// Deliver messages
	if ((ret = __rrr_udpstream_asd_queue_deliver_messages (
			&delivered_messages_count,
			session,
			queue,
			receive_callback,
			receive_callback_arg
	)) != 0) {
		RRR_MSG_0("Error while delivering messages in __rrr_udpstream_asd_deliver_and_maintain_queue \n");
		goto out;
	}

	// Update grace counters
	if ((ret = __rrr_udpstream_asd_queue_update_delivery_grace (
			&graced_messages_count,
			queue,
			delivered_messages_count
	)) != 0) {
		RRR_MSG_0("Error while updating grace in __rrr_udpstream_asd_deliver_and_maintain_queue \n");
		goto out;
	}

	// Reduce message traffic if we have many ACK handshakes to complete
	if ((ret = __rrr_udpstream_asd_queue_regulate_window_size (
			session,
			queue,
			graced_messages_count
	)) != 0) {
		RRR_MSG_0("Error while adjusting window sizes in __rrr_udpstream_asd_deliver_and_maintain_queue \n");
		goto out;
	}

	out:
	return ret;
}

// Deliver ready messages to application through callback function
int rrr_udpstream_asd_deliver_and_maintain_queues (
		struct rrr_udpstream_asd *session,
		int (*receive_callback)(struct rrr_msg_holder *message, void *arg),
		void *receive_callback_arg
) {
	int ret = 0;

	// Note : We could have deleted empty queues here, but they never get empty
	//        due to the grace time of delivered messages

	RRR_LL_ITERATE_BEGIN(&session->release_queues, struct rrr_udpstream_asd_queue_new);
		if ((ret = __rrr_udpstream_asd_deliver_and_maintain_queue (
				session,
				receive_callback,
				receive_callback_arg,
				node
		)) != 0) {
			RRR_MSG_0("ASD error while maintaining release queue for connect handle %u\n", node->source_connect_handle);
			ret = 1;
			goto out;
		}
	RRR_LL_ITERATE_END();

	out:
	return ret;
}

// Do all reading, sending and maintenance
int rrr_udpstream_asd_buffer_tick (
		int *receive_count,
		int *send_count,
		int (*allocator_callback)(RRR_UDPSTREAM_ALLOCATOR_CALLBACK_ARGS),
		void *allocator_callback_arg,
		struct rrr_udpstream_asd *session
) {
	int ret = 0;

	// TODO : Detect exhausted ID etc. and reconnect

	if ((ret = __rrr_udpstream_asd_buffer_connect_if_needed(session)) != 0) {
		if (ret == RRR_UDPSTREAM_ASD_NOT_READY) {
			// Connection not ready yet, do read and send tasks
		}
		else {
			RRR_MSG_0("Error from connect_if_needed in ASD connect handle %" PRIu32 "\n", session->connect_handle);
			goto out;
		}
	}

	if ((ret = __rrr_udpstream_asd_do_send_tasks(session)) != 0) {
		if (ret == RRR_UDPSTREAM_NOT_READY) {
			// Send buffer full
		}
		else {
			RRR_MSG_0("Error %i from UDP-stream while queuing messages to send of UDP-stream ASD handle %u\n",
					ret, session->connect_handle);
			ret = RRR_UDPSTREAM_ASD_HARD_ERR;
			goto out;
		}
	}

	if ((ret = rrr_udpstream_do_read_tasks(&session->udpstream, __rrr_udpstream_asd_control_frame_listener, session)) != 0) {
		if (ret != RRR_SOCKET_SOFT_ERROR) {
			RRR_MSG_0("Error from UDP-stream while reading data in receive_packets of UDP-stream ASD handle %u\n",
					session->connect_handle);
			ret = 1;
			goto out;
		}
		// Ignore soft error
	}

	if (rrr_udpstream_do_send_tasks(send_count, &session->udpstream) != 0) {
		RRR_MSG_0("UDP-stream send tasks failed in send_packets ASD\n");
		ret = 1;
		goto out;
	}

	if ((ret = __rrr_udpstream_asd_do_receive_tasks (
			receive_count,
			session,
			allocator_callback,
			allocator_callback_arg
	)) != 0) {
		RRR_MSG_0("Error from UDP-stream while receiving packets of UDP-stream ASD handle %u\n",
				session->connect_handle);
		ret = 1;
		goto out;
	}

	out:
	return ret;
}
