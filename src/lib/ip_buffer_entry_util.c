/*

Read Route Record

Copyright (C) 2018-2020 Atle Solbakken atle@goliathdns.no

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

#include "ip_buffer_entry_util.h"
#include "ip_buffer_entry_struct.h"

#include "log.h"
#include "mqtt/mqtt_topic.h"
#include "ip_buffer_entry.h"
#include "messages.h"

int rrr_ip_buffer_entry_util_new_with_empty_message (
		struct rrr_ip_buffer_entry **result,
		ssize_t message_data_length,
		const struct sockaddr *addr,
		socklen_t addr_len,
		int protocol
) {
	int ret = 0;

	struct rrr_ip_buffer_entry *entry = NULL;
	struct rrr_message *message = NULL;

	// XXX : Callers treat this function as message_data_length is an absolute value

	ssize_t message_size = sizeof(*message) - 1 + message_data_length;

	message = malloc(message_size);
	if (message == NULL) {
		RRR_MSG_0("Could not allocate message in ip_buffer_entry_new_with_message\n");
		goto out;
	}

	if (rrr_ip_buffer_entry_new (
			&entry,
			message_size,
			addr,
			addr_len,
			protocol,
			message
	) != 0) {
		RRR_MSG_0("Could not allocate ip buffer entry in ip_buffer_entry_new_with_message\n");
		ret = 1;
		goto out;
	}

	rrr_ip_buffer_entry_lock(entry);
	memset(message, '\0', message_size);
	rrr_ip_buffer_entry_unlock(entry);

	message = NULL;

	*result = entry;

	out:
	RRR_FREE_IF_NOT_NULL(message);
	return ret;
}

int rrr_ip_buffer_entry_util_clone_no_locking (
		struct rrr_ip_buffer_entry **result,
		const struct rrr_ip_buffer_entry *source
) {
	// Note : Do calculation correctly, not incorrect
	ssize_t message_data_length = source->data_length - (sizeof(struct rrr_message) - 1);

	if (message_data_length < 0) {
		RRR_BUG("Message too small in rrr_ip_buffer_entry_clone_no_locking\n");
	}

	int ret = rrr_ip_buffer_entry_util_new_with_empty_message (
			result,
			message_data_length,
			(struct sockaddr *) &source->addr,
			source->addr_len,
			source->protocol
	);

	if (ret == 0) {
		rrr_ip_buffer_entry_lock(*result);
		(*result)->send_time = source->send_time;
		memcpy((*result)->message, source->message, source->data_length);
		rrr_ip_buffer_entry_unlock(*result);
	}

	return ret;
}

int rrr_ip_buffer_entry_util_message_topic_match (
		int *does_match,
		const struct rrr_ip_buffer_entry *entry,
		const struct rrr_mqtt_topic_token *filter_first_token
) {
	const struct rrr_message *message = entry->message;

	int ret = 0;

	char *topic_tmp = NULL;

	*does_match = 0;

	struct rrr_mqtt_topic_token *entry_first_token = NULL;

	if ((rrr_slength) entry->data_length < (rrr_slength) sizeof(*message) || !MSG_IS_MSG(message) || MSG_TOPIC_LENGTH(message) == 0) {
		goto out;
	}

	if (rrr_mqtt_topic_validate_name_with_end (
			MSG_TOPIC_PTR(message),
			MSG_TOPIC_PTR(message) + MSG_TOPIC_LENGTH(message)
	) != 0) {
		RRR_MSG_0("Warning: Invalid syntax found in message while matching topic\n");
		ret = 0;
		goto out;
	}

	if (rrr_mqtt_topic_tokenize_with_end (
			&entry_first_token,
			MSG_TOPIC_PTR(message),
			MSG_TOPIC_PTR(message) + MSG_TOPIC_LENGTH(message)
	) != 0) {
		RRR_MSG_0("Tokenizing of topic failed in rrr_ip_buffer_entry_message_topic_match\n");
		ret = 1;
		goto out;
	}

	if ((ret = rrr_mqtt_topic_match_tokens_recursively(filter_first_token, entry_first_token)) != RRR_MQTT_TOKEN_MATCH) {
		if (RRR_DEBUGLEVEL_3) {
			if ((ret = rrr_message_topic_get(&topic_tmp, message)) != 0) {
				RRR_MSG_0("Could not get topic of message in rrr_ip_buffer_entry_util_message_topic_match while printing debug message\n");
				goto out;
			}
			RRR_MSG_3("Mismatched topic: '%s'\n", topic_tmp);
		}

		ret = 0;
		goto out;
	}

	*does_match = 1;

	out:
	RRR_FREE_IF_NOT_NULL(topic_tmp);
	rrr_mqtt_topic_token_destroy(entry_first_token);
	return ret;
}


