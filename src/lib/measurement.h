/*

Voltage Logger

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

#ifndef VL_MEASUREMENT_H
#define VL_MEASUREMENT_H

#include <stdint.h>
#include <stdlib.h>

#include "messages.h"

struct vl_message *reading_new (
		uint64_t reading_millis,
		uint64_t time
);

struct vl_message *reading_new_info (
		uint64_t time,
		const char *msg_terminated
);

#endif
