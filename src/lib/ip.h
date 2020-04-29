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

#ifndef RRR_IP_H
#define RRR_IP_H

#include <sys/socket.h>
#include <stdint.h>

#include "rrr_socket.h"
#include "linked_list.h"

#define RRR_IP_RECEIVE_OK 0
#define RRR_IP_RECEIVE_ERR 1
#define RRR_IP_RECEIVE_STOP 2

#define RRR_IP_RECEIVE_MAX_STEP_SIZE 8096

// Print/reset stats every X seconds
#define RRR_IP_STATS_DEFAULT_PERIOD 3

#define RRR_IP_AUTO	0
#define RRR_IP_UDP	1
#define RRR_IP_TCP	2

struct rrr_message;
struct rrr_array;
struct rrr_read_session_collection;
struct rrr_read_session;

struct ip_stats {
	pthread_mutex_t lock;
	unsigned int period;
	uint64_t time_from;
	unsigned long int packets;
	unsigned long int bytes;
	const char *type;
	const char *name;
};

struct ip_stats_twoway {
	struct ip_stats send;
	struct ip_stats receive;
};

struct rrr_ip_buffer_entry {
	ssize_t data_length;
	struct sockaddr addr;
	socklen_t addr_len;
	int protocol;
	uint64_t send_time;
	void *message;
};

struct rrr_ip_send_packet_info {
	void *data;
	int fd;
	struct addrinfo *res;
	int packet_counter;
};

struct rrr_ip_data {
	int fd;
	unsigned int port;
};

struct rrr_ip_accept_data {
	RRR_LL_NODE(struct rrr_ip_accept_data);
	struct rrr_sockaddr addr;
	socklen_t len;
	struct rrr_ip_data ip_data;
};

struct rrr_ip_accept_data_collection {
	RRR_LL_HEAD(struct rrr_ip_accept_data);
};

#define RRR_IP_STATS_UPDATE_OK 0		// Stats was updated
#define RRR_IP_STATS_UPDATE_ERR 1	// Error
#define RRR_IP_STATS_UPDATE_READY 2	// Limit is reached, we should print

void rrr_ip_buffer_entry_destroy (
		struct rrr_ip_buffer_entry *entry
);
void rrr_ip_buffer_entry_destroy_void (
		void *entry
);
void rrr_ip_buffer_entry_set_message_dangerous (
		struct rrr_ip_buffer_entry *entry,
		void *message,
		ssize_t data_length
);
int rrr_ip_buffer_entry_new (
		struct rrr_ip_buffer_entry **result,
		ssize_t data_length,
		const struct sockaddr *addr,
		socklen_t addr_len,
		int protocol,
		void *message
);
int rrr_ip_buffer_entry_new_with_empty_message (
		struct rrr_ip_buffer_entry **result,
		ssize_t message_data_length,
		const struct sockaddr *addr,
		socklen_t addr_len,
		int protocol
);
int rrr_ip_buffer_entry_clone (
		struct rrr_ip_buffer_entry **result,
		const struct rrr_ip_buffer_entry *source
);
int rrr_ip_stats_init (
		struct ip_stats *stats, unsigned int period, const char *type, const char *name
);
int rrr_ip_stats_init_twoway (
		struct ip_stats_twoway *stats, unsigned int period, const char *name
);
int rrr_ip_stats_update (
		struct ip_stats *stats, unsigned long int packets, unsigned long int bytes
);
int rrr_ip_stats_print_reset (
		struct ip_stats *stats, int do_reset
);
int rrr_ip_receive_array (
		struct rrr_read_session_collection *read_session_collection,
		int fd,
		int read_flags,
		const struct rrr_array *definition,
		int do_sync_byte_by_byte,
		int (*callback)(struct rrr_ip_buffer_entry *entry, void *arg),
		void *arg,
		struct ip_stats *stats
);
int rrr_ip_send (
	int *err,
	int fd,
	const struct sockaddr *sockaddr,
	socklen_t addrlen,
	void *data,
	ssize_t data_size
);
int rrr_ip_send_message (
		const struct rrr_message* message,
		struct rrr_ip_send_packet_info* info,
		struct ip_stats *stats
);
void rrr_ip_network_cleanup (void *arg);
int rrr_ip_network_start_udp_ipv4_nobind (struct rrr_ip_data *data);
int rrr_ip_network_start_udp_ipv4 (struct rrr_ip_data *data);
int rrr_ip_network_sendto_udp_ipv4_or_ipv6 (
		struct rrr_ip_data *ip_data,
		unsigned int port,
		const char *host,
		void *data,
		ssize_t size
);
int rrr_ip_network_connect_tcp_ipv4_or_ipv6_raw (
		struct rrr_ip_accept_data **accept_data,
		struct sockaddr *addr,
		socklen_t addr_len
);
int rrr_ip_network_connect_tcp_ipv4_or_ipv6 (struct rrr_ip_accept_data **accept_data, unsigned int port, const char *host);
int rrr_ip_network_start_tcp_ipv4_and_ipv6 (struct rrr_ip_data *data, int max_connections);
int rrr_ip_close (struct rrr_ip_data *data);
int rrr_ip_accept(struct rrr_ip_accept_data **accept_data,
		struct rrr_ip_data *listen_data, const char *creator, int tcp_nodelay);
void rrr_ip_accept_data_close_and_destroy (struct rrr_ip_accept_data *accept_data);
void rrr_ip_accept_data_close_and_destroy_void (void *accept_data);
void rrr_ip_accept_data_collection_clear(struct rrr_ip_accept_data_collection *collection);
void rrr_ip_accept_data_collection_clear_void(void *collection);
void rrr_ip_accept_data_collection_close_and_remove(
		struct rrr_ip_accept_data_collection *collection,
		struct sockaddr *sockaddr,
		socklen_t socklen
);
void rrr_ip_accept_data_collection_close_and_remove_by_fd (
		struct rrr_ip_accept_data_collection *collection,
		int fd
);
struct rrr_ip_accept_data *rrr_ip_accept_data_collection_find (
		struct rrr_ip_accept_data_collection *collection,
		struct sockaddr *sockaddr,
		socklen_t socklen
);
struct rrr_ip_accept_data *rrr_ip_accept_data_collection_find_by_fd (
		struct rrr_ip_accept_data_collection *collection,
		int fd
);

#endif
