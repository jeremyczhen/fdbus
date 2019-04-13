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

#if defined(CONFIG_DEBUG_LOG)
#if defined(CONFIG_LOG_TO_STDOUT)
#include <stdio.h>
#define FDB_TLOG_D(_tag, ...) fprintf(stdout, __VA_ARGS__)
#define FDB_TLOG_I(_tag, ...) fprintf(stdout, __VA_ARGS__)
#define FDB_TLOG_W(_tag, ...) fprintf(stdout, __VA_ARGS__)
#define FDB_TLOG_E(_tag, ...) fprintf(stderr, __VA_ARGS__)
#define FDB_TLOG_F(_tag, ...) fprintf(stderr, __VA_ARGS__)
#else /* CONFIG_LOG_TO_STDOUT */

#include "CFdbContext.h"
#include "CLogProducer.h"

#define FDB_TLOG_D(_tag, ...) do { \
    CLogProducer *logger = FDB_CONTEXT->getLogger(); \
    if (logger) { \
        logger->logTrace(FDB_LL_DEBUG, (_tag), __VA_ARGS__); \
    } \
} while (0)

#define FDB_TLOG_I(_tag, ...) do { \
    CLogProducer *logger = FDB_CONTEXT->getLogger(); \
    if (logger) { \
        logger->logTrace(FDB_LL_INFO, (_tag), __VA_ARGS__); \
    } \
} while (0)

#define FDB_TLOG_W(_tag, ...) do { \
    CLogProducer *logger = FDB_CONTEXT->getLogger(); \
    if (logger) { \
        logger->logTrace(FDB_LL_WARNING, (_tag), __VA_ARGS__); \
    } \
} while (0)

#define FDB_TLOG_E(_tag, ...) do { \
    CLogProducer *logger = FDB_CONTEXT->getLogger(); \
    if (logger) { \
        logger->logTrace(FDB_LL_ERROR, (_tag), __VA_ARGS__); \
    } \
} while (0)

#define FDB_TLOG_F(_tag, ...) do { \
    CLogProducer *logger = FDB_CONTEXT->getLogger(); \
    if (logger) { \
        logger->logTrace(FDB_LL_FATAL, (_tag), __VA_ARGS__); \
    } \
} while (0)

#endif /* CONFIG_LOG_TO_STDOUT */
#else
#define FDB_TLOG_D(_tag, ...)
#define FDB_TLOG_I(_tag, ...)
#define FDB_TLOG_W(_tag, ...)
#define FDB_TLOG_E(_tag, ...)
#define FDB_TLOG_F(_tag, ...)
#endif

#if defined(FDB_LOG_TAG)
#define FDB_LOG_D(...) FDB_TLOG_D(FDB_LOG_TAG, __VA_ARGS__)
#define FDB_LOG_I(...) FDB_TLOG_I(FDB_LOG_TAG, __VA_ARGS__)
#define FDB_LOG_W(...) FDB_TLOG_W(FDB_LOG_TAG, __VA_ARGS__)
#define FDB_LOG_E(...) FDB_TLOG_E(FDB_LOG_TAG, __VA_ARGS__)
#define FDB_LOG_F(...) FDB_TLOG_F(FDB_LOG_TAG, __VA_ARGS__)
#else
#define FDB_LOG_D(...) FDB_TLOG_D(CLogProducer::mTagName.c_str(), __VA_ARGS__)
#define FDB_LOG_I(...) FDB_TLOG_I(CLogProducer::mTagName.c_str(), __VA_ARGS__)
#define FDB_LOG_W(...) FDB_TLOG_W(CLogProducer::mTagName.c_str(), __VA_ARGS__)
#define FDB_LOG_E(...) FDB_TLOG_E(CLogProducer::mTagName.c_str(), __VA_ARGS__)
#define FDB_LOG_F(...) FDB_TLOG_F(CLogProducer::mTagName.c_str(), __VA_ARGS__)
#endif

#define setFdbLogTag(_tag_name) CLogProducer::mTagName = _tag_name

#endif
