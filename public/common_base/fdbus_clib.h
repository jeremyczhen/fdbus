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

#ifndef __FDBUS_CLIB_H__
#define __FDBUS_CLIB_H__

#include "fdbus_server.h"
#include "fdbus_client.h"
#include "fdb_log_trace.h"

#ifdef __cplusplus
extern "C"
{
#endif

LIB_EXPORT
fdb_bool_t fdb_start();

LIB_EXPORT
void fdb_log_trace(enum EFdbLogLevel level, const char *tag, const char *log_data);

#ifdef __cplusplus
}
#endif

#endif
