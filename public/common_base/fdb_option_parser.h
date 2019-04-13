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

#ifndef _OPTION_PARSE_H_
#define _OPTION_PARSE_H_

#ifdef  __cplusplus
extern "C" {
#endif

enum fdb_option_type {
	FDB_OPTION_INTEGER,
	FDB_OPTION_UNSIGNED_INTEGER,
	FDB_OPTION_STRING,
	FDB_OPTION_BOOLEAN
};

struct fdb_option {
	enum fdb_option_type type;
	const char *name;
	int short_name;
	void *data;
};

int
fdb_parse_options(const struct fdb_option *options,
	      int count, int *argc, char *argv[]);
char **strsplit(const char* str, const char* delim, unsigned int *numtokens);
void endstrsplit(char **tokens, unsigned int numtokens);

#ifdef  __cplusplus
}
#endif

#endif
