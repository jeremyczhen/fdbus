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

#ifndef __FDB_LOG_TRACE_H__
#define __FDB_LOG_TRACE_H__

#ifdef __cplusplus
extern "C"
{
#endif

void fdb_log_debug(const char *tag, ...);
void fdb_log_info(const char *tag, ...);
void fdb_log_warning(const char *tag, ...);
void fdb_log_error(const char *tag, ...);
void fdb_log_fatal(const char *tag, ...);

#ifdef __cplusplus
}
#endif

#if defined(CONFIG_DEBUG_LOG)
#if defined(CONFIG_LOG_TO_STDOUT)
#include <stdio.h>
#define FDB_TLOG_D(_tag, ...) fprintf(stdout, __VA_ARGS__)
#define FDB_TLOG_I(_tag, ...) fprintf(stdout, __VA_ARGS__)
#define FDB_TLOG_W(_tag, ...) fprintf(stdout, __VA_ARGS__)
#define FDB_TLOG_E(_tag, ...) fprintf(stderr, __VA_ARGS__)
#define FDB_TLOG_F(_tag, ...) fprintf(stderr, __VA_ARGS__)
#else /* CONFIG_LOG_TO_STDOUT */
#define FDB_TLOG_D(_tag, ...) fdb_log_debug(_tag,   __VA_ARGS__)
#define FDB_TLOG_I(_tag, ...) fdb_log_info(_tag,    __VA_ARGS__)
#define FDB_TLOG_W(_tag, ...) fdb_log_warning(_tag, __VA_ARGS__)
#define FDB_TLOG_E(_tag, ...) fdb_log_error(_tag,   __VA_ARGS__)
#define FDB_TLOG_F(_tag, ...) fdb_log_fatal(_tag,   __VA_ARGS__)
#endif /* CONFIG_LOG_TO_STDOUT */
#else
#define FDB_TLOG_D(_tag, ...)
#define FDB_TLOG_I(_tag, ...)
#define FDB_TLOG_W(_tag, ...)
#define FDB_TLOG_E(_tag, ...)
#define FDB_TLOG_F(_tag, ...)
#endif

#if !defined(FDB_LOG_TAG)
#define FDB_LOG_TAG "unknown"
#endif

#define FDB_LOG_D(...) FDB_TLOG_D(FDB_LOG_TAG, __VA_ARGS__)
#define FDB_LOG_I(...) FDB_TLOG_I(FDB_LOG_TAG, __VA_ARGS__)
#define FDB_LOG_W(...) FDB_TLOG_W(FDB_LOG_TAG, __VA_ARGS__)
#define FDB_LOG_E(...) FDB_TLOG_E(FDB_LOG_TAG, __VA_ARGS__)
#define FDB_LOG_F(...) FDB_TLOG_F(FDB_LOG_TAG, __VA_ARGS__)

#endif
