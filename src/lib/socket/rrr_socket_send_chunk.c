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
#include <stdio.h>
#include <string.h>

#include "../log.h"
#include "rrr_socket_send_chunk.h"
#include "rrr_socket.h"
#include "../util/macro_utils.h"

struct rrr_socket_send_chunk {
	RRR_LL_NODE(struct rrr_socket_send_chunk);
	void *data;
	ssize_t data_size;
	ssize_t data_pos;
};

static void __rrr_socket_send_chunk_destroy (
		struct rrr_socket_send_chunk *chunk
) {
	RRR_FREE_IF_NOT_NULL(chunk->data);
	free(chunk);
}

void rrr_socket_send_chunk_collection_clear (
		struct rrr_socket_send_chunk_collection *target
) {
	RRR_LL_DESTROY(target, struct rrr_socket_send_chunk, __rrr_socket_send_chunk_destroy(node));
}

int rrr_socket_send_chunk_collection_push (
		struct rrr_socket_send_chunk_collection *target,
		void **data,
		ssize_t data_size
) {
	int ret = 0;

	if (data_size < 0) {
		RRR_BUG("BUG: Data size was < 0 in rrr_socket_send_chunk_collection_push\n");
	}

	struct rrr_socket_send_chunk *new_chunk = NULL;

	if ((new_chunk = malloc(sizeof(*new_chunk))) == NULL) {
		RRR_MSG_0("Could not allocate memory in __rrr_socket_send_chunk_collection_push\n");
		ret = 1;
		goto out;
	}

	memset(new_chunk, '\0', sizeof(*new_chunk));

	new_chunk->data_size = data_size;
	new_chunk->data = *data;
	*data = NULL;

	RRR_LL_APPEND(target, new_chunk);

	out:
	return ret;
}

int rrr_socket_send_chunk_collection_push_const (
		struct rrr_socket_send_chunk_collection *target,
		const void *data,
		ssize_t data_size
) {
	int ret = 0;

	void *data_copy = malloc(data_size);
	if (data_copy == NULL) {
		RRR_MSG_0("Could not allocate memory in rrr_socket_send_chunk_collection_push_const\n");
		ret = 1;
		goto out;
	}

	memcpy(data_copy, data, data_size);

	ret = rrr_socket_send_chunk_collection_push (target, &data_copy, data_size);

	out:
	RRR_FREE_IF_NOT_NULL(data_copy);
	return ret;
}

int rrr_socket_send_chunk_collection_sendto (
		struct rrr_socket_send_chunk_collection *chunks,
		int fd,
		const struct sockaddr *addr,
		socklen_t addr_len
) {
	int ret = 0;

	RRR_LL_ITERATE_BEGIN(chunks, struct rrr_socket_send_chunk);
		RRR_DBG_7("Chunk non-blocking send on fd %i, pos/size %lld/%lld\n",
			fd,  (long long int) node->data_pos, (long long int) node->data_size);

		ssize_t written_bytes = 0;
		if ((ret = rrr_socket_sendto_nonblock_check_retry (
			&written_bytes,
			fd,
			node->data + node->data_pos,
			node->data_size - node->data_pos,
			addr,
			addr_len
		)) != 0) {
			if (ret == RRR_SOCKET_WRITE_INCOMPLETE) {
				node->data_pos += written_bytes;
			}
			goto out;
		}
		RRR_LL_ITERATE_SET_DESTROY(); // Chunk complete
	RRR_LL_ITERATE_END_CHECK_DESTROY(chunks, 0; __rrr_socket_send_chunk_destroy(node));

	out:
	return ret;
}

int rrr_socket_send_chunk_collection_sendto_with_callback (
		struct rrr_socket_send_chunk_collection *chunks,
		int (*callback)(ssize_t *written_bytes, const void *data, ssize_t data_size, void *arg),
		void *callback_arg
) {
	int ret = 0;

	RRR_LL_ITERATE_BEGIN(chunks, struct rrr_socket_send_chunk);
		RRR_DBG_7("Chunk send with callback pos/size %lld/%lld\n",
			(long long int) node->data_pos, (long long int) node->data_size);

		ssize_t written_bytes = 0;

		ret = callback (
				&written_bytes,
				node->data + node->data_pos,
				node->data_size - node->data_pos,
				callback_arg
		) &~ RRR_SOCKET_WRITE_INCOMPLETE;

		node->data_pos += written_bytes;
		if (node->data_pos > node->data_size) {
			RRR_BUG("BUG: Too many bytes written in rrr_socket_send_chunk_collection_sendto_with_callback\n");
		}
		else if (node->data_pos == node->data_size) {
			RRR_LL_ITERATE_SET_DESTROY(); // Chunk complete
		}

		if (ret != 0 || written_bytes == 0) {
			RRR_LL_ITERATE_LAST();
		}
	RRR_LL_ITERATE_END_CHECK_DESTROY(chunks, 0; __rrr_socket_send_chunk_destroy(node));

	return ret;
}
