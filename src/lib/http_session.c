/*
#include <http_part.h>

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

#include <stdlib.h>
#include <string.h>

#include "../global.h"
#include "gnu.h"
#include "base64.h"
#include "linked_list.h"
#include "http_fields.h"
#include "http_session.h"
#include "http_util.h"
#include "http_part.h"
#include "rrr_socket.h"
#include "rrr_socket_read.h"

void rrr_http_session_destroy (struct rrr_http_session *session) {
	RRR_FREE_IF_NOT_NULL(session->host);
	RRR_FREE_IF_NOT_NULL(session->endpoint);
	RRR_FREE_IF_NOT_NULL(session->user_agent);
	rrr_http_fields_collection_clear(&session->fields);
	if (session->data != NULL) {
		rrr_http_part_destroy(session->data);
	}
	rrr_socket_read_session_collection_clear(&session->read_sessions);
	free(session);
}

int rrr_http_session_new (
		struct rrr_http_session **target,
		int fd,
		enum rrr_http_method method,
		const char *host,
		const char *endpoint,
		const char *user_agent
) {
	int ret = 0;

	*target = NULL;

	struct rrr_http_session *session = malloc(sizeof(*session));
	if (session == NULL) {
		VL_MSG_ERR("Could not allocate memory in rrr_http_session_new\n");
		ret = 1;
		goto out;
	}

	memset(session, '\0', sizeof(*session));

	session->method = method;
	session->fd = fd;

	session->host = strdup(host);
	if (session->host == NULL) {
		VL_MSG_ERR("Could not allocate memory in rrr_http_session_new A\n");
		ret = 1;
		goto out;
	}

	if (endpoint != NULL && *endpoint != '\0') {
		session->endpoint = strdup(endpoint);
	}
	else {
		session->endpoint = strdup("/");
	}

	if (session->endpoint == NULL) {
		VL_MSG_ERR("Could not allocate memory in rrr_http_session_new B\n");
		ret = 1;
		goto out;
	}

	if (user_agent != NULL && *user_agent != '\0') {
		session->user_agent = strdup(user_agent);
		if (session->user_agent == NULL) {
			VL_MSG_ERR("Could not allocate memory in rrr_http_session_new D\n");
			ret = 1;
			goto out;
		}
	}

	rrr_socket_read_session_collection_init(&session->read_sessions);

	*target = session;
	session = NULL;

	out:
	if (session != NULL) {
		rrr_http_session_destroy(session);
	}
	return ret;
}

int rrr_http_session_add_query_field (
		struct rrr_http_session *session,
		const char *name,
		const char *value
) {
	return rrr_http_fields_collection_add_field(&session->fields, name, value);
}

static int __rrr_http_session_send_post_body (struct rrr_http_session *session) {
	int ret = 0;
	char *boundary_buf = NULL;
	char *name_buf = NULL;
	char *body_buf = NULL;

	// RFC7578

	if ((ret = rrr_asprintf (&boundary_buf, "rrr-boundary-%i", rand())) < 0) {
		VL_MSG_ERR("Could not create boundary_buf string in __rrr_http_session_send_post_body\n");
		ret = 1;
		goto out;
	}

	if ((ret = rrr_asprintf (
			&body_buf,
			"Content-Type: multipart/form-data; boundary=%s\r\n", // <-- ONE CRLF
			boundary_buf
	)) < 0) {
		VL_MSG_ERR("Could not create content type string in __rrr_http_session_send_post_body\n");
		goto out;
	}

	if ((ret = rrr_socket_sendto(session->fd, body_buf, strlen(body_buf), NULL, 0)) != 0) {
		VL_MSG_ERR("Could not send first part of HTTP request in __rrr_http_session_send_post_body\n");
		goto out;
	}

	RRR_LINKED_LIST_ITERATE_BEGIN(&session->fields, struct rrr_http_field);
		RRR_FREE_IF_NOT_NULL(name_buf);
		if ((name_buf = rrr_http_util_quote_header_value(node->name, '"', '"')) == NULL) {
			VL_MSG_ERR("Could not quote field name_buf in __rrr_http_session_send_post_body\n");
			ret = 1;
			goto out;
		}

		// TODO : Support binary stuff

		RRR_FREE_IF_NOT_NULL(body_buf);
		if ((ret = rrr_asprintf (
				&body_buf,
				"\r\n--%s\r\n"  // <-- ONE CRLF
				"Content-Disposition: form-data; name=%s\r\n\r\n%s",
				boundary_buf,
				name_buf,
				node->value
		)) < 0) {
			VL_MSG_ERR("Could not create content type string and body  in __rrr_http_session_send_post_body\n");
			ret = 1;
			goto out;
		}

		if ((ret = rrr_socket_sendto(session->fd, body_buf, strlen(body_buf), NULL, 0)) != 0) {
			VL_MSG_ERR("Could not send form part of HTTP request in __rrr_http_session_send_post_body\n");
			goto out;
		}
	RRR_LINKED_LIST_ITERATE_END();

	RRR_FREE_IF_NOT_NULL(body_buf);
	if ((ret = rrr_asprintf (
			&body_buf,
			"\r\n--%s--",  // <-- ONE CRLF AFTER BODY
			boundary_buf
	)) < 0) {
		VL_MSG_ERR("Could not create last boundary in __rrr_http_session_send_post_body\n");
		ret = 1;
		goto out;
	}

	if ((ret = rrr_socket_sendto(session->fd, body_buf, strlen(body_buf), NULL, 0)) != 0) {
		VL_MSG_ERR("Could not send last part of HTTP request in __rrr_http_session_send_post_body\n");
		goto out;
	}

	out:
	RRR_FREE_IF_NOT_NULL(name_buf);
	RRR_FREE_IF_NOT_NULL(boundary_buf);
	RRR_FREE_IF_NOT_NULL(body_buf);

	return ret;
}


static int __rrr_http_session_send_get_body (struct rrr_http_session *session) {
	int ret = 0;
	char *body_buf = NULL;
	char *final_buf = NULL;

	if ((body_buf = rrr_http_fields_to_urlencoded_form_data(&session->fields)) == NULL) {
		VL_MSG_ERR("Could not create GET body in __rrr_http_session_send_get_body\n");
		ret = 1;
		goto out;
	}

	if ((ret = rrr_asprintf (
			&final_buf,
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"Content-Length: %u\r\n\r\n%s",
			strlen(body_buf),
			body_buf
	)) < 0) {
		VL_MSG_ERR("Could not create content type string in __rrr_http_session_send_get_body\n");
		ret = 1;
		goto out;
	}

	if ((ret = rrr_socket_sendto(session->fd, final_buf, strlen(final_buf), NULL, 0)) != 0) {
		VL_MSG_ERR("Could not send GET body in __rrr_http_session_send_get_body\n");
		goto out;
	}

	out:
	RRR_FREE_IF_NOT_NULL(final_buf);
	RRR_FREE_IF_NOT_NULL(body_buf);
	return ret;
}

int rrr_http_session_send_request (
		struct rrr_http_session *session
) {
	int ret = 0;

	char *request_buf = NULL;
	char *host_buf = NULL;
	char *user_agent_buf = NULL;

	host_buf = rrr_http_util_quote_header_value(session->host, '"', '"');
	if (host_buf == NULL) {
		VL_MSG_ERR("Invalid host '%s' in rrr_http_session_send_request\n", session->host);
		ret = 1;
		goto out;
	}

	user_agent_buf = rrr_http_util_quote_header_value(session->user_agent, '"', '"');
	if (user_agent_buf == NULL) {
		VL_MSG_ERR("Invalid user agent '%s' in rrr_http_session_send_request\n", session->user_agent);
		ret = 1;
		goto out;
	}

	if ((ret = rrr_asprintf (
			&request_buf,
			"%s %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"User-Agent: %s\r\n"
			"Accept-Charset: UTF-8\r\n",
			(session->method == RRR_HTTP_METHOD_POST ? "POST" : "GET"),
			session->endpoint,
			host_buf,
			user_agent_buf
	)) < 0) {
		VL_MSG_ERR("Error while making request string in rrr_http_session_send_request\n");
		ret = 1;
		goto out;
	}

	if ((ret = rrr_socket_sendto(session->fd, request_buf, strlen(request_buf), NULL, 0)) != 0) {
		VL_MSG_ERR("Could not send first part of HTTP request in rrr_http_session_send_request\n");
		goto out;
	}

	if (RRR_LINKED_LIST_COUNT(&session->fields) == 0) {
		if ((ret = rrr_socket_sendto(session->fd, "\r\n", strlen("\r\n"), NULL, 0)) != 0) {
			VL_MSG_ERR("Could not send last \r\n rrr_http_session_send_request\n");
			goto out;
		}
	}
	else if (session->method == RRR_HTTP_METHOD_POST) {
		if ((ret = __rrr_http_session_send_post_body (session)) != 0) {
			VL_MSG_ERR("Could not send POST body in rrr_http_session_send_request\n");
			goto out;
		}
	}
	else {
		if ((ret = __rrr_http_session_send_get_body (session)) != 0) {
			VL_MSG_ERR("Could not send GET body in rrr_http_session_send_request\n");
			goto out;
		}
	}

	out:
	RRR_FREE_IF_NOT_NULL(user_agent_buf);
	RRR_FREE_IF_NOT_NULL(host_buf);
	RRR_FREE_IF_NOT_NULL(request_buf);
	return ret;
}

struct rrr_http_session_receive_data {
	struct rrr_http_session *session;
	ssize_t parse_complete_pos;
	int (*callback)(struct rrr_http_session *session, void *arg);
	void *callback_arg;
};

static int __rrr_http_session_receive_callback (
		struct rrr_socket_read_session *read_session, void *arg
) {
	struct rrr_http_session_receive_data *receive_data = arg;

	const char *data_start = read_session->rx_buf_ptr + receive_data->parse_complete_pos;
	const char *data_end = read_session->rx_buf_ptr + read_session->rx_buf_wpos;

	if (data_end > data_start) {
		receive_data->session->data->data_ptr = data_start;
		receive_data->session->data->data_length = data_end - data_start;
	}

	return receive_data->callback(receive_data->session, receive_data->callback_arg);
}

static int __rrr_http_session_receive_get_total_size (
		struct rrr_socket_read_session *read_session, void *arg
) {
	struct rrr_http_session_receive_data *receive_data = arg;

	int ret = 0;

	const char *start = read_session->rx_buf_ptr + receive_data->parse_complete_pos;

	ssize_t parsed_bytes = 0;
	ret = rrr_http_part_parse (
			receive_data->session->data,
			&parsed_bytes,
			start,
			read_session->rx_buf_ptr + read_session->rx_buf_wpos
	);

	receive_data->parse_complete_pos += parsed_bytes;

	if (ret == RRR_HTTP_PARSE_OK) {
		read_session->target_size = receive_data->parse_complete_pos + receive_data->session->data->data_length;
	}
	else if (ret == RRR_HTTP_PARSE_INCOMPLETE) {
		ret = RRR_SOCKET_READ_INCOMPLETE;
	}
	else if (ret == RRR_HTTP_PARSE_UNTIL_CLOSE) {
		read_session->read_complete_method = RRR_SOCKET_READ_COMPLETE_METHOD_CONN_CLOSE;
		ret = RRR_SOCKET_OK;
	}
	else {
		ret = RRR_SOCKET_SOFT_ERROR;
	}

	return ret;
}

int rrr_http_session_receive (
		struct rrr_http_session *session,
		int (*callback)(struct rrr_http_session *session, void *arg),
		void *callback_arg
) {
	int ret = 0;

	if (session->data != NULL) {
		rrr_http_part_destroy(session->data);
	}

	if ((ret = rrr_http_part_new(&session->data)) != 0) {
		VL_MSG_ERR("Could not create HTTP part in rrr_http_session_receive\n");
		goto out;
	}

	struct rrr_http_session_receive_data callback_data = {
			session,
			0,
			callback,
			callback_arg
	};

	for (int i = 1000; i >= 0; i--) {
		ret = rrr_socket_read_message (
				&session->read_sessions,
				session->fd,
				4096,
				65535,
				RRR_SOCKET_READ_METHOD_RECV | RRR_SOCKET_READ_USE_TIMEOUT,
				__rrr_http_session_receive_get_total_size,
				&callback_data,
				__rrr_http_session_receive_callback,
				&callback_data
		);

		if (ret == RRR_SOCKET_OK) {
			// TODO : Check for persistent connection/more results which might be
			//		  stored in read session overshoot buffer
			goto out;
		}
		else if (ret != RRR_SOCKET_READ_INCOMPLETE) {
			VL_MSG_ERR("Error while reading from server in rrr_http_session_receive\n");
			ret = 1;
			goto out;
		}
	}

	out:
	return ret;
}