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

#ifndef RRR_MQTT_ID_POOL_H
#define RRR_MQTT_ID_POOL_H

#include <stdint.h>
#include <pthread.h>
#include <stdio.h>

// This is the maximum for 16 bit identifiers
#define RRR_MQTT_ID_POOL_SIZE_IN_32 2048

struct rrr_mqtt_id_pool {
	uint32_t *pool;
	pthread_mutex_t lock;
	ssize_t allocated_size;
	uint16_t last_allocated_id;
};

int rrr_mqtt_id_pool_init (struct rrr_mqtt_id_pool *pool);
void rrr_mqtt_id_pool_clear (struct rrr_mqtt_id_pool *pool);
void rrr_mqtt_id_pool_destroy (struct rrr_mqtt_id_pool *pool);
uint16_t rrr_mqtt_id_pool_get_id (struct rrr_mqtt_id_pool *pool);
void rrr_mqtt_id_pool_release_id (struct rrr_mqtt_id_pool *pool, uint16_t id);

#endif /* RRR_MQTT_ID_POOL_H */
