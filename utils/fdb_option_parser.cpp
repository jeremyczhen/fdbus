/*
 * Copyright (C) 2015   Jeremy Chen jeremy_cz@yahoo.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <common_base/fdb_option_parser.h>
#include "fdb_string_helpers.h"

static int
handle_option(const struct fdb_option *option, char *value)
{
	char* p;

	switch (option->type) {
	case FDB_OPTION_INTEGER:
		if (safe_strtoint(value, (int32_t *)option->data))
			return 0;
		return 1;
	case FDB_OPTION_UNSIGNED_INTEGER:
		errno = 0;
		* (uint32_t *) option->data = (uint32_t)strtoul(value, &p, 10);
		if (errno != 0 || p == value || *p != '\0')
			return 0;
		return 1;
	case FDB_OPTION_STRING:
		* (char **) option->data = strdup(value);
		return 1;
	default:
		assert(0);
	}

        return 0;
}

static int
long_option(const struct fdb_option *options, int count, char *arg)
{
	int k, len;

	for (k = 0; k < count; k++) {
		if (!options[k].name)
			continue;

		len = (int)strlen(options[k].name);
		if (strncmp(options[k].name, arg + 2, (size_t)len) != 0)
			continue;

		if (options[k].type == FDB_OPTION_BOOLEAN) {
			if (!arg[len + 2]) {
				* (int32_t *) options[k].data = 1;

				return 1;
			}
		} else if (arg[len+2] == '=') {
			return handle_option(options + k, arg + len + 3);
		}
	}

	return 0;
}

static int
short_option(const struct fdb_option *options, int count, char *arg)
{
	int k;

	if (!arg[1])
		return 0;

	for (k = 0; k < count; k++) {
		if (options[k].short_name != arg[1])
			continue;

		if (options[k].type == FDB_OPTION_BOOLEAN) {
			if (!arg[2]) {
				* (int32_t *) options[k].data = 1;

				return 1;
			}
		} else if (arg[2]) {
			return handle_option(options + k, arg + 2);
		} else {
			return 0;
		}
	}

	return 0;
}

static int
short_option_with_arg(const struct fdb_option *options, int count, char *arg, char *param)
{
	int k;

	if (!arg[1])
		return 0;

	for (k = 0; k < count; k++) {
		if (options[k].short_name != arg[1])
			continue;

		if (options[k].type == FDB_OPTION_BOOLEAN)
			continue;

		return handle_option(options + k, param);
	}

	return 0;
}

int
fdb_parse_options(const struct fdb_option *options,
	      int count, int *argc, char *argv[])
{
	int i, j;

	for (i = 1, j = 1; i < *argc; i++) {
		if (argv[i][0] == '-') {
			if (argv[i][1] == '-') {
				/* Long option, e.g. --foo or --foo=bar */
				if (long_option(options, count, argv[i]))
					continue;

			} else {
				/* Short option, e.g -f or -f42 */
				if (short_option(options, count, argv[i]))
					continue;

				/* ...also handle -f 42 */
				if (i+1 < *argc &&
				    short_option_with_arg(options, count, argv[i], argv[i+1])) {
					i++;
					continue;
				}
			}
		}
		argv[j++] = argv[i];
	}
	argv[j] = NULL;
	*argc = j;

	return j;
}

char **strsplit(const char* str, const char* delim, unsigned int *numtokens) {
    // copy the original string so that we don't overwrite parts of it
    // (don't do this if you don't need to keep the old line,
    // as this is less efficient)
    char *s = strdup(str);
    // these three variables are part of a very common idiom to
    // implement a dynamically-growing array
    size_t tokens_alloc = 1;
    size_t tokens_used = 0;
    char **tokens = (char **)calloc(tokens_alloc, sizeof(char*));
    char *token;
    char *strtok_ctx = 0;
#ifdef __WIN32__
    for (token = strtok_s(s, delim, &strtok_ctx);
            token != NULL;
            token = strtok_s(NULL, delim, &strtok_ctx)) {
        // check if we need to allocate more space for tokens
        if (tokens_used == tokens_alloc) {
            tokens_alloc *= 2;
            tokens = (char **)realloc(tokens, tokens_alloc * sizeof(char*));
        }
        tokens[tokens_used++] = strdup(token);
    }
#else
    for (token = strtok_r(s, delim, &strtok_ctx);
            token != NULL;
            token = strtok_r(NULL, delim, &strtok_ctx)) {
        // check if we need to allocate more space for tokens
        if (tokens_used == tokens_alloc) {
            tokens_alloc *= 2;
            tokens = (char **)realloc(tokens, tokens_alloc * sizeof(char*));
        }
        tokens[tokens_used++] = strdup(token);
    }
#endif
    // cleanup
    if (tokens_used == 0) {
        free(tokens);
        tokens = NULL;
    } else {
        tokens = (char **)realloc(tokens, tokens_used * sizeof(char*));
    }
    *numtokens = (unsigned int)tokens_used;
    free(s);
    return tokens;
}

void endstrsplit(char **tokens, unsigned int numtokens)
{
    unsigned int i;

    if (!tokens)
        return;

    for (i = 0; i < numtokens; i++) {
        free(tokens[i]);
    }
    free(tokens);
}
