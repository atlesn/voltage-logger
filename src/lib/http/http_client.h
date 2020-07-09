/*

Read Route Record

Copyright (C) 2019-2020 Atle Solbakken atle@goliathdns.no

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

#ifndef RRR_HTTP_CLIENT_H
#define RRR_HTTP_CLIENT_H

#include <inttypes.h>
#include <sys/types.h>

#include "http_common.h"

#define RRR_HTTP_CLIENT_FINAL_CALLBACK_ARGS				\
		struct rrr_http_client_data *data, 				\
		int response_code,								\
		const char *response_argument,					\
		int chunk_idx,									\
		int chunk_total,								\
		const char *data_start,							\
		ssize_t data_size,								\
		void *arg

#define RRR_HTTP_CLIENT_BEFORE_SEND_CALLBACK_ARGS		\
		char **query_string,							\
		struct rrr_http_session *session,				\
		void *arg

struct rrr_net_transport_config;
struct rrr_http_client_config;
struct rrr_http_session;

struct rrr_http_client_data {
	enum rrr_http_transport transport_force;

	char *server;
	uint16_t http_port;
	char *endpoint;
	char *user_agent;

	int ssl_no_cert_verify;

	int do_retry;
};

struct rrr_http_client_request_callback_data {
	enum rrr_http_method method;

	int response_code;
	char *response_argument;

	// Errors do not propagate through net transport framework. Return
	// value of http callbacks is saved here.
	int http_receive_ret;

	struct rrr_http_client_data *data;

	int (*before_send_callback)(RRR_HTTP_CLIENT_BEFORE_SEND_CALLBACK_ARGS);
	void *before_send_callback_arg;
	int (*final_callback)(RRR_HTTP_CLIENT_FINAL_CALLBACK_ARGS);
	void *final_callback_arg;
};

int rrr_http_client_data_init (
		struct rrr_http_client_data *data,
		const char *user_agent
);
int rrr_http_client_data_reset (
		struct rrr_http_client_data *data,
		const struct rrr_http_client_config *config,
		enum rrr_http_transport transport_force
);
void rrr_http_client_data_cleanup (
		struct rrr_http_client_data *data
);
// Note that data in the struct may change if there are any redirects
int rrr_http_client_send_request (
		struct rrr_http_client_data *data,
		enum rrr_http_method method,
		const struct rrr_net_transport_config *net_transport_config,
		int (*before_send_callback)(RRR_HTTP_CLIENT_BEFORE_SEND_CALLBACK_ARGS),
		void *before_send_callback_arg,
		int (*final_callback)(RRR_HTTP_CLIENT_FINAL_CALLBACK_ARGS),
		void *final_callback_arg
);


#endif /* RRR_HTTP_CLIENT_H */
