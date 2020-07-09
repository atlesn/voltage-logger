/*

Read Route Record

Copyright (C) 2020 Atle Solbakken atle@goliathdns.no

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

#include <string.h>
#include <strings.h>

#include "net_transport_config.h"

#include "../../global.h"
#include "../log.h"
#include "../gnu.h"
#include "../instance_config.h"
#include "../global.h"

void rrr_net_transport_config_cleanup (
		struct rrr_net_transport_config *data
) {
	RRR_FREE_IF_NOT_NULL(data->tls_certificate_file);
	RRR_FREE_IF_NOT_NULL(data->tls_key_file);
	RRR_FREE_IF_NOT_NULL(data->tls_ca_file);
	RRR_FREE_IF_NOT_NULL(data->tls_ca_path);
	RRR_FREE_IF_NOT_NULL(data->transport_type_str);
}

int rrr_net_transport_config_parse (
		struct rrr_net_transport_config *data,
		struct rrr_instance_config *config,
		const char *prefix,
		int allow_both_transport_type
) {
	int ret = 0;

	RRR_INSTANCE_CONFIG_PREFIX_BEGIN(prefix);

	RRR_INSTANCE_CONFIG_STRING_SET("_tls_certificate_file");
	RRR_INSTANCE_CONFIG_PARSE_OPTIONAL_UTF8_DEFAULT_NULL(config_string, tls_certificate_file);

	RRR_INSTANCE_CONFIG_STRING_SET("_tls_key_file");
	RRR_INSTANCE_CONFIG_PARSE_OPTIONAL_UTF8_DEFAULT_NULL(config_string, tls_key_file);

	RRR_INSTANCE_CONFIG_STRING_SET("_tls_ca_file");
	RRR_INSTANCE_CONFIG_PARSE_OPTIONAL_UTF8_DEFAULT_NULL(config_string, tls_ca_file);

	RRR_INSTANCE_CONFIG_STRING_SET("_tls_ca_path");
	RRR_INSTANCE_CONFIG_PARSE_OPTIONAL_UTF8_DEFAULT_NULL(config_string, tls_ca_path);

	RRR_INSTANCE_CONFIG_STRING_SET("_transport_type");
	RRR_INSTANCE_CONFIG_PARSE_OPTIONAL_UTF8_DEFAULT_NULL(config_string, transport_type_str);

	if (	(data->tls_certificate_file != NULL && data->tls_key_file == NULL) ||
			(data->tls_certificate_file == NULL && data->tls_key_file != NULL)
	) {
		RRR_MSG_0("Only one of mqtt_certificate_file and mqtt_key_file was specified, either both or none are required in instance %s",
				config->name);
		ret = 1;
		goto out;
	}

	if (data->transport_type_str != NULL) {
		if (strcasecmp(data->transport_type_str, "plain") == 0) {
			data->transport_type = RRR_NET_TRANSPORT_PLAIN;
		}
		else if (strcasecmp(data->transport_type_str, "tls") == 0) {
			data->transport_type = RRR_NET_TRANSPORT_TLS;
		}
		else if (allow_both_transport_type && strcasecmp(data->transport_type_str, "both") == 0) {
			data->transport_type = RRR_NET_TRANSPORT_BOTH;
		}
		else {
			RRR_MSG_0("Unknown value '%s' for mqtt_transport_type in instance %s\n",
					data->transport_type_str, config->name);
			ret = 1;
			goto out;
		}
	}
	else {
		data->transport_type = RRR_NET_TRANSPORT_PLAIN;
	}

	// Note : It's allowed not to specify a certificate
	if (data->tls_certificate_file != NULL && data->transport_type == RRR_NET_TRANSPORT_TLS) {
		RRR_MSG_0("TLS certificate specified in mqtt_certificate_file but mqtt_transport_type was not 'tls' for mqttclient instance %s\n",
				config->name);
		ret = 1;
		goto out;
	}

	RRR_INSTANCE_CONFIG_PREFIX_END();

	return ret;
}