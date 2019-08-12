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

#include <stdlib.h>
#include <string.h>

#include "../global.h"
#include "mqtt_session.h"

void rrr_mqtt_session_collection_destroy (struct rrr_mqtt_session_collection *target) {
	if (target->private_data != NULL) {
		VL_BUG("private data was not NULL in rrr_mqtt_session_collection_destroy\n");
	}
	free(target);
}

int rrr_mqtt_session_collection_new (
		struct rrr_mqtt_session_collection **target,
		const struct rrr_mqtt_session_collection_methods *methods
) {
	int ret = 0;

	*target = NULL;

	struct rrr_mqtt_session_collection *res = malloc(sizeof(*res));
	if (res == NULL) {
		VL_MSG_ERR("Could not allocate memory in rrr_mqtt_session_collection_new\n");
		ret = 1;
		goto out;
	}

	memset (res, '\0', sizeof(*res));

	res->methods = methods;

	*target = res;

	out:
	return ret;
}