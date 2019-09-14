/*

Read Route Record

Copyright (C) 2019 Atle Solbakken atle@goliathdns.no

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

#include <stdio.h>

#include "../global.h"
#include "crc32.h"
#include "rrr_socket.h"
#include "rrr_socket_msg.h"

void rrr_socket_msg_populate_head (
		struct rrr_socket_msg *message,
		vl_u16 type,
		vl_u32 msg_size,
		vl_u64 value
) {
	if (msg_size < sizeof(*message)) {
		VL_BUG("Size was too small in rrr_socket_msg_head_to_network\n");
	}

	message->network_size = msg_size;
	message->msg_type = type;
	message->msg_size = msg_size;
	message->msg_value = value;
}

void rrr_socket_msg_checksum_and_to_network_endian (
		struct rrr_socket_msg *message
) {
	// HEX dumper
/*	for (unsigned int i = 0; i < message->msg_size; i++) {
		unsigned char *buf = (unsigned char *) message;
		printf("%02x-", *(buf+i));
	}
	printf("\n");*/

	message->header_crc32 = 0;
	message->data_crc32 = 0;

	char *data_begin = ((char *) message) + sizeof(*message);
	ssize_t data_size = message->network_size - sizeof(*message);

	if (data_size > 0) {
		message->data_crc32 = crc32buf(data_begin, data_size);
	}

//	printf ("Put crc32 %lu data size %li\n", message->data_crc32, message->network_size - sizeof(*message));

	message->network_size = htobe32(message->network_size);
	message->msg_type = htobe16(message->msg_type);
	message->msg_size = htobe32(message->msg_size);
	message->msg_value = htobe64(message->msg_value);
	message->data_crc32 = htobe32(message->data_crc32);

	char *head_begin = ((char *) message) + sizeof(message->header_crc32);
	ssize_t head_size = sizeof(*message) - sizeof(message->header_crc32);

	message->header_crc32 = htobe32(crc32buf(head_begin, head_size));
}

static int __rrr_socket_msg_head_validate (
		struct rrr_socket_msg *message,
		ssize_t expected_size
) {
	int ret = 0;

	if ((ssize_t) message->network_size != expected_size) {
		VL_MSG_ERR("Message network size mismatch in __rrr_socket_msg_head_validate actual size is %li stated size is %" PRIu32 "\n",
				expected_size, message->msg_size);
		ret = 1;
		goto out;
	}

	if (RRR_SOCKET_MSG_IS_CTRL(message)) {
		// Clear all known control flags
		vl_u16 type = message->msg_type;
		type = type & ~(RRR_SOCKET_MSG_CTRL_F_ALL);
		if (type != 0) {
			VL_MSG_ERR("Unknown control flags in message: %u\n", type);
			ret = 1;
			goto out;
		}
	}
	else if (RRR_SOCKET_MSG_IS_SETTING(message) || RRR_SOCKET_MSG_IS_VL_MESSAGE(message)) {
		// OK
	}
	else {
		VL_MSG_ERR("Received message with invalid type %u in __rrr_socket_msg_head_validate\n", message->msg_type);
		ret = 1;
		goto out;
	}

	out:
	return ret;
}

int rrr_socket_msg_head_to_host_and_verify (
		struct rrr_socket_msg *message,
		ssize_t expected_size
) {
	message->header_crc32 = 0;
	message->network_size = be32toh(message->network_size);
	message->data_crc32 = be32toh(message->data_crc32);
	message->msg_type = be16toh(message->msg_type);
	message->msg_size = be32toh(message->msg_size);
	message->msg_value = be64toh(message->msg_value);

	if (__rrr_socket_msg_head_validate (message, expected_size) != 0) {
		VL_MSG_ERR("Received socket message was invalid in rrr_socket_msg_head_to_host\n");
		return 1;
	}

	return 0;
}

int rrr_socket_msg_get_target_size_and_check_checksum (
		ssize_t *target_size,
		struct rrr_socket_msg *socket_msg,
		ssize_t buf_size
) {
	if (buf_size < (ssize_t) sizeof(struct rrr_socket_msg)) {
		return RRR_SOCKET_READ_INCOMPLETE;
	}

	*target_size = 0;

	if (crc32cmp (
			((char*) socket_msg) + sizeof(socket_msg->header_crc32),
			sizeof(*socket_msg) - sizeof(socket_msg->header_crc32),
			be32toh(socket_msg->header_crc32)
	) != 0) {
		return RRR_SOCKET_SOFT_ERROR;
	}

	*target_size = be32toh(socket_msg->network_size);

	return RRR_SOCKET_OK;
}

int rrr_socket_msg_check_data_checksum_and_length (
		struct rrr_socket_msg *message,
		ssize_t data_size
) {
	if (data_size < (ssize_t) sizeof(*message)) {
		VL_BUG("rrr_socket_msg_checksum_check called with too short message\n");
	}
	if (message->msg_size != data_size) {
		VL_MSG_ERR("Message size mismatch in rrr_socket_msg_checksum_check\n");
		return 1;
	}
	// HEX dumper
/*	for (unsigned int i = 0; i < data_size; i++) {
		unsigned char *buf = (unsigned char *) message;
		printf("%02x-", *(buf+i));
	}
	printf("\n");
	printf ("Check crc32 %lu data size %li\n", message->data_crc32, data_size - sizeof(*message));*/

	vl_u32 checksum = message->data_crc32;

	char *data_begin = ((char *) message) + sizeof(*message);
	return crc32cmp(data_begin, data_size - sizeof(*message), checksum) != 0;
}
