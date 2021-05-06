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

#ifndef RRR_SHM_STRUCT_H
#define RRR_SHM_STRUCT_H

#include <pthread.h>

#define RRR_SHM_COLLECTION_MAX 192

struct rrr_shm {
	char name[8];
	size_t data_size;
	unsigned int version_ptr;
};

struct rrr_shm_ptr {
	void *ptr;
	size_t data_size;
	unsigned int version_ptr;
};

struct rrr_shm_collection_master {
	pthread_mutex_t lock;
	unsigned int version_master;
	struct rrr_shm elements[RRR_SHM_COLLECTION_MAX];
};

struct rrr_shm_collection_slave {
	struct rrr_shm_collection_master *master;
	unsigned int version_master;
	struct rrr_shm_ptr ptrs[RRR_SHM_COLLECTION_MAX];
};

#define RRR_SHM_COLLECTION_MASTER_INIT { PTHREAD_MUTEX_INITIALIZER, 0, {{"", 0, 0}} }
#define RRR_SHM_COLLECTION_SLAVE_INIT(master) { master, 0, {{0}} }

#endif /* RRR_SHM_STRUCT_H */
