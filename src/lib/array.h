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

#ifndef RRR_ARRAY_H
#define RRR_ARRAY_H

#include <stdint.h>

#include "cmdlineparser/cmdline.h"
#include "instance_config.h"
#include "linked_list.h"
#include "type.h"

#define RRR_ARRAY_VERSION 4

struct vl_message;

struct rrr_array_value_packed {
	rrr_type type;
	rrr_type_length total_length;
	rrr_type_length elements;
	char data[1];
} __attribute((packed));

struct rrr_array {
		RRR_LINKED_LIST_HEAD(struct rrr_type_value);
};

int rrr_array_parse_definition (
		struct rrr_array *target,
		struct rrr_instance_config *config,
		const char *cmd_key
);
int rrr_array_parse_data_from_definition (
		struct rrr_array *target,
		const char *data,
		const rrr_type_length length
);
int rrr_array_definition_collection_clone (
		struct rrr_array *target,
		const struct rrr_array *source
) ;
void rrr_array_clear (
		struct rrr_array *collection
);
struct rrr_type_value *rrr_array_value_get_by_index (
		struct rrr_array *definition,
		int idx
);
int rrr_array_new_message (
		struct vl_message **final_message,
		const struct rrr_array *definition,
		uint64_t time
);
int rrr_array_message_to_collection (
		struct rrr_array *target,
		const struct vl_message *message_orig
);

#endif /* RRR_ARRAY_H */
