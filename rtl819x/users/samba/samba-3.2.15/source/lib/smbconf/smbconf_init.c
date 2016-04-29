/*
 *  Unix SMB/CIFS implementation.
 *  libsmbconf - Samba configuration library, init dispatcher
 *  Copyright (C) Michael Adam 2008
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "smbconf_private.h"

#define INCLUDES_VALNAME "includes"


/**
 * smbconf initialization dispatcher
 *
 * this takes a configuration source in the form of
 * backend:path and calles the appropriate backend
 * init function with the path argument
 *
 * known backends:
 * -  "registry" or "reg"
 * -  "txt" or "file"
 */
WERROR smbconf_init(TALLOC_CTX *mem_ctx, struct smbconf_ctx **conf_ctx,
		    const char *source)
{
	WERROR werr;
	char *backend = NULL;
	char *path = NULL;
	char *sep;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	if (conf_ctx == NULL) {
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	if ((source == NULL) || (*source == '\0')) {
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	backend = talloc_strdup(tmp_ctx, source);
	if (backend == NULL) {
		werr = WERR_NOMEM;
		goto done;
	}

	sep = strchr(backend, ':');
	if (sep != NULL) {
		*sep = '\0';
		path = sep + 1;
		if (strlen(path) == 0) {
			path = NULL;
		}
	}

#if 0
	if (strequal(backend, "registry") || strequal(backend, "reg")) {
		werr = smbconf_init_reg(mem_ctx, conf_ctx, path);
	} else 
#endif
	  if (strequal(backend, "file") || strequal(backend, "txt")) {
		werr = smbconf_init_txt(mem_ctx, conf_ctx, path);
	} else if (sep == NULL) {
		/*
		 * If no separator was given in the source, and the string is
		 * not a know backend, assume file backend and use the source
		 * string as a path argument.
		 */
		werr = smbconf_init_txt(mem_ctx, conf_ctx, backend);
	} else {
		/*
		 * Separator was specified but this is not a known backend.
		 * Can't handle this.
		 */
		DEBUG(1, ("smbconf_init: ERROR - unknown backend '%s' given\n",
			  backend));
		werr = WERR_INVALID_PARAM;
	}

done:
	TALLOC_FREE(tmp_ctx);
	return werr;
}
