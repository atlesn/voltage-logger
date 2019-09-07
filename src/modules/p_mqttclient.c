/*

Read Route Record

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>

#include "../lib/mqtt_topic.h"
#include "../lib/mqtt_client.h"
#include "../lib/mqtt_common.h"
#include "../lib/mqtt_session_ram.h"
#include "../lib/mqtt_subscription.h"
#include "../lib/poll_helper.h"
#include "../lib/instance_config.h"
#include "../lib/settings.h"
#include "../lib/instances.h"
#include "../lib/messages.h"
#include "../lib/threads.h"
#include "../lib/buffer.h"
#include "../lib/vl_time.h"
#include "../lib/ip.h"
#include "../lib/utf8.h"
#include "../lib/linked_list.h"
#include "../global.h"

#define RRR_MQTT_DEFAULT_SERVER_PORT 1883
#define RRR_MQTT_DEFAULT_QOS 1
#define RRR_MQTT_DEFAULT_VERSION 4 // 3.1.1

struct mqtt_client_data {
	struct instance_thread_data *thread_data;
	struct fifo_buffer output_buffer;
	struct rrr_mqtt_client_data *mqtt_client_data;
	rrr_setting_uint server_port;
	struct rrr_mqtt_subscription_collection *subscriptions;
	struct rrr_mqtt_property_collection connect_properties;
	char *server;
	char *publish_topic;
	char *version_str;
	char *client_identifier;
	uint8_t qos;
	uint8_t version;
	struct rrr_mqtt_conn *connection;
};

static int poll_callback(struct fifo_callback_args *poll_data, char *data, unsigned long int size) {
	struct instance_thread_data *thread_data = poll_data->source;
//	struct mqtt_data *private_data = thread_data->private_data;
	struct vl_message *reading = (struct vl_message *) data;
	VL_DEBUG_MSG_2 ("mqtt: Result from buffer: measurement %" PRIu64 " size %lu, discarding data\n", reading->data_numeric, size);

	free(data);

	// fifo_buffer_write(&private_data->output_buffer, data, size);

	return 0;
}

static void data_cleanup(void *arg) {
	struct mqtt_client_data *data = arg;
	fifo_buffer_invalidate(&data->output_buffer);
	RRR_FREE_IF_NOT_NULL(data->server);
	RRR_FREE_IF_NOT_NULL(data->publish_topic);
	RRR_FREE_IF_NOT_NULL(data->version_str);
	RRR_FREE_IF_NOT_NULL(data->client_identifier);
	rrr_mqtt_subscription_collection_destroy(data->subscriptions);
	rrr_mqtt_property_collection_destroy(&data->connect_properties);
}

static int data_init (
		struct mqtt_client_data *data,
		struct instance_thread_data *thread_data
) {
	memset(data, '\0', sizeof(*data));
	int ret = 0;

	data->thread_data = thread_data;

	ret |= fifo_buffer_init(&data->output_buffer);

	if (ret != 0) {
		VL_MSG_ERR("Could not initialize fifo buffer in mqtt client data_init\n");
		goto out;
	}

	if (rrr_mqtt_subscription_collection_new(&data->subscriptions) != 0) {
		VL_MSG_ERR("Could not create subscription collection in mqtt client data_init\n");
		goto out;
	}

	out:
	if (ret != 0) {
		data_cleanup(data);
	}

	return ret;
}

static int parse_sub_topic (const char *topic_str, void *arg) {
	struct mqtt_client_data *data = arg;

	if (rrr_mqtt_topic_filter_validate_name(topic_str) != 0) {
		return 1;
	}

	if (rrr_mqtt_subscription_collection_push_unique_str (
			data->subscriptions,
			topic_str,
			0,
			0,
			0,
			data->qos
	) != 0) {
		VL_MSG_ERR("Could not add topic '%s' to subscription collection\n", topic_str);
		return 1;
	}

	return 0;
}

// TODO : Provide more configuration arguments
static int parse_config (struct mqtt_client_data *data, struct rrr_instance_config *config) {
	int ret = 0;

	rrr_setting_uint mqtt_port = 0;
	rrr_setting_uint mqtt_qos = 0;

	if ((ret = rrr_instance_config_read_unsigned_integer(&mqtt_port, config, "mqtt_server_port")) == 0) {
		// OK
	}
	else if (ret != RRR_SETTING_NOT_FOUND) {
		VL_MSG_ERR("Error while parsing mqtt_server_port setting of instance %s\n", config->name);
		ret = 1;
		goto out;
	}
	else {
		mqtt_port = RRR_MQTT_DEFAULT_SERVER_PORT;
		ret = 0;
	}
	data->server_port = mqtt_port;

	if ((ret = rrr_instance_config_read_unsigned_integer(&mqtt_qos, config, "mqtt_qos")) == 0) {
		if (mqtt_qos > 2) {
			VL_MSG_ERR("Setting mqtt_qos was >2 in config of instance %s\n", config->name);
			ret = 1;
			goto out;
		}
	}
	else if (ret != RRR_SETTING_NOT_FOUND) {
		VL_MSG_ERR("Error while parsing mqtt_qos setting of instance %s\n", config->name);
		ret = 1;
		goto out;
	}
	else {
		mqtt_qos = RRR_MQTT_DEFAULT_QOS;
		ret = 0;
	}
	data->qos = (uint8_t) mqtt_qos;

	if ((ret = rrr_instance_config_get_string_noconvert_silent(&data->client_identifier, config, "mqtt_client_identifier")) != 0) {
		data->client_identifier = malloc(strlen(config->name) + 1);
		if (data->client_identifier == NULL) {
			VL_MSG_ERR("Could not allocate memory in parse_config of instance %s\n", config->name);
		}
		strcpy(data->client_identifier, config->name);
	}

	if (rrr_utf8_validate(data->client_identifier, strlen(data->client_identifier)) != 0) {
		VL_MSG_ERR("Client identifier of mqtt client instance %s was not valid UTF-8\n", config->name);
		ret = 1;
		goto out;
	}

	if ((ret = rrr_instance_config_get_string_noconvert_silent(&data->version_str, config, "mqtt_version")) != 0) {
		data->version = 3;
	}
	else {
		if (strcmp(data->version_str, "3.1.1") == 0) {
			data->version = 4;
		}
		else if (strcmp (data->version_str, "5") == 0) {
			data->version = 5;
		}
		else {
			VL_MSG_ERR("Unknown protocol version '%s' in setting mqtt_version of instance %s. " \
					"Supported values are 3.1.1 and 5\n", data->version_str, config->name);
			ret = 1;
			goto out;
		}
	}

	if ((ret = rrr_instance_config_get_string_noconvert(&data->server, config, "mqtt_server")) != 0) {
		VL_MSG_ERR("Error while parsing mqtt_server setting of instance %s\n", config->name);
		ret = 1;
		goto out;
	}

	if ((ret = rrr_instance_config_get_string_noconvert_silent(&data->publish_topic, config, "mqtt_publish_topic")) == 0) {
		if (strlen(data->publish_topic) == 0) {
			VL_MSG_ERR("Topic name in mqtt_publish_topic was empty for instance %s\n", config->name);
			ret = 1;
			goto out;
		}
		if (rrr_mqtt_topic_validate_name(data->publish_topic) != 0) {
			VL_MSG_ERR("Topic name in mqtt_publish_topic was invalid for instance %s\n", config->name);
			ret = 1;
			goto out;
		}
	}

	if ((ret = rrr_instance_config_traverse_split_commas_silent_fail(config, "mqtt_subscribe_topics", parse_sub_topic, data)) != 0) {
		VL_MSG_ERR("Error while parsing mqtt_subscribe_topics setting of instance %s\n", config->name);
		ret = 1;
		goto out;
	}

	/* On error, memory is freed by data_cleanup */

	out:
	return ret;
}

static int poll_delete (RRR_MODULE_POLL_SIGNATURE) {
	struct mqtt_client_data *client_data = data->private_data;
	return fifo_read_clear_forward(&client_data->output_buffer, NULL, callback, poll_data, wait_milliseconds);
}

static int poll_keep (RRR_MODULE_POLL_SIGNATURE) {
	struct mqtt_client_data *client_data = data->private_data;
	return fifo_search(&client_data->output_buffer, callback, poll_data, wait_milliseconds);
}

static int process_suback_subscription (
		struct mqtt_client_data *data,
		struct rrr_mqtt_subscription *subscription,
		const int i,
		const uint8_t qos_or_reason_v5
) {
	int ret = 0;

	if (qos_or_reason_v5 > 2) {
		const struct rrr_mqtt_p_reason *reason = rrr_mqtt_p_reason_get_v5(qos_or_reason_v5);
		if (reason == NULL) {
			VL_MSG_ERR("Unknown reason 0x%02x from mqtt broker in SUBACK topic index %i in mqtt client instance %s",
					qos_or_reason_v5, i, INSTANCE_D_NAME(data->thread_data));
			return 1;
		}
		VL_MSG_ERR("Warning: Subscription '%s' index '%i' rejected from broker in mqtt client instance %s with reason '%s'\n",
				subscription->topic_filter,
				i,
				INSTANCE_D_NAME(data->thread_data),
				reason->description
		);
	}
	else if (qos_or_reason_v5 < subscription->qos_or_reason_v5) {
		VL_MSG_ERR("Warning: Subscription '%s' index '%i' assigned QoS %u from server while %u was requested in mqtt client instance %s \n",
				subscription->topic_filter,
				i,
				qos_or_reason_v5,
				subscription->qos_or_reason_v5,
				INSTANCE_D_NAME(data->thread_data)
		);
	}

	return ret;
}

static int process_suback(struct rrr_mqtt_client_data *mqtt_client_data, struct rrr_mqtt_p *packet, void *arg) {
	struct mqtt_client_data *data = arg;

	(void)(mqtt_client_data);

	if (RRR_MQTT_P_GET_TYPE(packet) != RRR_MQTT_P_TYPE_SUBACK) {
		VL_BUG("Unknown packet of type %u received in mqtt client process_suback\n",
				RRR_MQTT_P_GET_TYPE(packet));
	}

	struct rrr_mqtt_p_suback *suback = (struct rrr_mqtt_p_suback *) packet;

	int orig_count = rrr_mqtt_subscription_collection_count(data->subscriptions);
	int new_count = suback->acknowledgements_size;

	if (orig_count != new_count) {
		// Session framework should catch this
		VL_BUG("Count mismatch in SUBSCRIBE and SUBACK messages in mqtt client instance %s (%i vs %i)\n",
				INSTANCE_D_NAME(data->thread_data), orig_count, new_count);
	}

	for (int i = 0; i < new_count; i++) {
		struct rrr_mqtt_subscription *subscription;
		subscription = rrr_mqtt_subscription_collection_get_subscription_by_idx (
				data->subscriptions,
				i
		);
		if (process_suback_subscription(data, subscription, i, suback->acknowledgements[i]) != 0) {
			return 1;
		}
	}

	return 0;
}

static int receive_publish (struct rrr_mqtt_p_publish *publish, void *arg) {
	int ret = 0;

	struct mqtt_client_data *data = arg;
	struct vl_message *message = NULL;

	int did_init = 0;
	if (publish->payload != NULL) {
		RRR_MQTT_P_LOCK(publish->payload);
	 	if (publish->payload->length > 0) {
	 			if (message_new_empty (
	 					&message,
	 					MSG_TYPE_MSG,
						0,
						MSG_CLASS_POINT,
						publish->create_time,
						publish->create_time,
						0,
						publish->payload->length
				) != 0) {
	 				VL_MSG_ERR("Could not initialize message in receive_publish of mqtt client instance %s (A)\n",
	 						INSTANCE_D_NAME(data->thread_data));
	 				ret = 1;
	 				goto unlock_payload;
	 			}

	 			memcpy(message->data_, publish->payload->payload_start, publish->payload->length);

	 			did_init = 1;
	 	}

	 	unlock_payload:
	 	RRR_MQTT_P_UNLOCK(publish->payload);
	 	if (ret != 0) {
	 		goto out;
	 	}
	}

 	if (did_init == 0) {
		if (message_new_empty (
				&message,
				MSG_TYPE_MSG,
				0,
				MSG_CLASS_POINT,
				publish->create_time,
				publish->create_time,
				0,
				0
		) != 0) {
			VL_MSG_ERR("Could not initialize message in receive_publish of mqtt client instance %s (B)\n",
					INSTANCE_D_NAME(data->thread_data));
			ret = 1;
			goto unlock_payload;
		}
 	}

 	fifo_buffer_write(&data->output_buffer, (char*) message, sizeof(*message));
 	message = NULL;

	out:
	RRR_FREE_IF_NOT_NULL(message);
	return ret;
}

static void *thread_entry_mqtt_client (struct vl_thread *thread) {
	struct instance_thread_data *thread_data = thread->private_data;
	struct mqtt_client_data *data = thread_data->private_data = thread_data->private_memory;
	struct poll_collection poll;

	int init_ret = 0;
	if ((init_ret = data_init(data, thread_data)) != 0) {
		VL_MSG_ERR("Could not initalize data in mqtt client instance %s flags %i\n",
			INSTANCE_D_NAME(thread_data), init_ret);
		pthread_exit(0);
	}

	VL_DEBUG_MSG_1 ("mqtt client thread data is %p\n", thread_data);

	poll_collection_init(&poll);
	pthread_cleanup_push(poll_collection_clear_void, &poll);
	pthread_cleanup_push(data_cleanup, data);
	pthread_cleanup_push(thread_set_stopping, thread);

	thread_set_state(thread, VL_THREAD_STATE_INITIALIZED);
	thread_signal_wait(thread_data->thread, VL_THREAD_SIGNAL_START);
	thread_set_state(thread, VL_THREAD_STATE_RUNNING);

	if (parse_config(data, thread_data->init_data.instance_config) != 0) {
		VL_MSG_ERR("Configuration parse failed for mqtt client instance '%s'\n", thread_data->init_data.module->instance_name);
		goto out_message;
	}

	rrr_instance_config_check_all_settings_used(thread_data->init_data.instance_config);

	struct rrr_mqtt_common_init_data init_data = {
		INSTANCE_D_NAME(thread_data),
		RRR_MQTT_COMMON_RETRY_INTERVAL,
		RRR_MQTT_COMMON_CLOSE_WAIT_TIME,
		RRR_MQTT_COMMON_MAX_CONNECTIONS
	};

	if (rrr_mqtt_client_new (
			&data->mqtt_client_data,
			&init_data,
			rrr_mqtt_session_collection_ram_new,
			NULL,
			process_suback,
			data
		) != 0) {
		VL_MSG_ERR("Could not create new mqtt client\n");
		goto out_message;
	}

	pthread_cleanup_push(rrr_mqtt_client_destroy_void, data->mqtt_client_data);

	poll_add_from_thread_senders_ignore_error(&poll, thread_data, RRR_POLL_POLL_DELETE);

	if (poll_collection_count(&poll) > 0) {
		if (data->publish_topic == NULL || *(data->publish_topic) == '\0') {
			VL_MSG_ERR("mqtt client instance %s has senders specified but no publish topic (mqtt_publish_topic) is set in configuration\n",
					INSTANCE_D_NAME(thread_data));
			goto out_destroy_client;
		}
	}
	else {
		if (data->publish_topic != NULL) {
			VL_MSG_ERR("mqtt client instance %s has publish topic set but there are not senders specified in configuration\n",
					INSTANCE_D_NAME(thread_data));
			goto out_destroy_client;
		}
	}

	VL_DEBUG_MSG_1 ("mqtt client started thread %p\n", thread_data);

	if (rrr_mqtt_property_collection_add_uint32(
			&data->connect_properties,
			RRR_MQTT_PROPERTY_RECEIVE_MAXIMUM,
			0xffffffff
	) != 0) {
		VL_MSG_ERR("Could not set CONNECT properties in mqtt client instance %s\n",
				INSTANCE_D_NAME(thread_data));
		goto out_destroy_client;
	}

	reconnect:
	for (int i = 20; i >= 0 && thread_check_encourage_stop(thread_data->thread) != 1; i--) {
		VL_DEBUG_MSG_1("MQTT client instance %s attempting to connect to server '%s' port '%llu'\n",
				INSTANCE_D_NAME(thread_data), data->server, data->server_port);
		if (rrr_mqtt_client_connect (
				&data->connection,
				data->mqtt_client_data,
				data->server,
				data->server_port,
				data->version,
				RRR_MQTT_CLIENT_KEEP_ALIVE,
				0, // <-- Clean start
				&data->connect_properties
		) != 0) {
			if (i == 0) {
				VL_MSG_ERR("Could not connect to mqtt server '%s' port %llu in instance %s\n",
						data->server, data->server_port, INSTANCE_D_NAME(thread_data));
				goto out_destroy_client;
			}
			usleep (100000);
		}
		else {
			break;
		}
	}

	int subscriptions_sent = 0;
	while (thread_check_encourage_stop(thread_data->thread) != 1) {
		update_watchdog_time(thread_data->thread);

		// TODO : Figure out what to do with data from local senders

		int alive = 0;
		if (rrr_mqtt_client_connection_is_alive(&alive, data->mqtt_client_data, data->connection)) {
			VL_MSG_ERR("Error in mqtt client instance %s while checking for connection alive\n",
					INSTANCE_D_NAME(thread_data));
			break;
		}
		if (alive == 0) {
			VL_DEBUG_MSG_1("Connection lost for mqtt client instance %s, reconnecting\n",
					INSTANCE_D_NAME(thread_data));
			goto reconnect;
		}

		if (poll_do_poll_delete_simple (&poll, thread_data, poll_callback, 50) != 0) {
			break;
		}

		if (rrr_mqtt_client_synchronized_tick(data->mqtt_client_data) != 0) {
			VL_MSG_ERR("Error int mqtt client instance %s while running tasks\n",
					INSTANCE_D_NAME(thread_data));
			break;
		}

		if (rrr_mqtt_client_iterate_and_clear_local_delivery(data->mqtt_client_data, receive_publish, data) != 0) {
			VL_MSG_ERR("Error while iterating local delivery queue in mqtt client instance %s\n",
					INSTANCE_D_NAME(thread_data));
			break;
		}

		if (subscriptions_sent == 0) {
			if (rrr_mqtt_client_subscribe (
					data->mqtt_client_data,
					data->connection,
					data->subscriptions
			) != 0) {
				VL_MSG_ERR("Could not subscribe to topics in mqtt client instance %s\n",
						INSTANCE_D_NAME(thread_data));
				goto reconnect;
			}
			subscriptions_sent = 1;
		}

		usleep (5000); // 50 ms
	}

	out_destroy_client:
		pthread_cleanup_pop(1);
	out_message:
		VL_DEBUG_MSG_1 ("Thread mqtt client %p exiting\n", thread_data->thread);
		pthread_cleanup_pop(1);
		pthread_cleanup_pop(1);
		pthread_cleanup_pop(1);
		pthread_exit(0);
}

static struct module_operations module_operations = {
		NULL,
		thread_entry_mqtt_client,
		NULL,
		poll_keep,
		NULL,
		poll_delete,
		NULL,
		NULL,
		NULL,
		NULL
};

static const char *module_name = "mqtt_client";

__attribute__((constructor)) void load(void) {
}

void init(struct instance_dynamic_data *data) {
	data->private_data = NULL;
	data->module_name = module_name;
	data->type = VL_MODULE_TYPE_FLEXIBLE;
	data->operations = module_operations;
	data->dl_ptr = NULL;
	data->start_priority = VL_THREAD_START_PRIORITY_NETWORK;
}

void unload(void) {
	VL_DEBUG_MSG_1 ("Destroy mqtt client module\n");
}

