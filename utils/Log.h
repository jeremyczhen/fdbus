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

#ifndef BASE_INCLUDE_LOG_LOG_H
#define BASE_INCLUDE_LOG_LOG_H

#ifndef ANDROID
#define _FDB_LOG_TAG_ "FDBUS"
#include <common_base/fdb_log_trace.h>
#define LOG_D(...) FDB_TLOG_D(_FDB_LOG_TAG_, __VA_ARGS__)
#define LOG_I(...) FDB_TLOG_I(_FDB_LOG_TAG_, __VA_ARGS__)
#define LOG_W(...) FDB_TLOG_W(_FDB_LOG_TAG_, __VA_ARGS__)
#define LOG_E(...) FDB_TLOG_E(_FDB_LOG_TAG_, __VA_ARGS__)
#define LOG_F(...) FDB_TLOG_F(_FDB_LOG_TAG_, __VA_ARGS__)

#else /* build for Android */
#include <android/log.h>

#ifndef LOG_TAG
#define LOG_TAG "FDBUS"
#endif

#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG  , LOG_TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO   , LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN   , LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR  , LOG_TAG, __VA_ARGS__)

#define LOG_D ALOGD
#define LOG_I ALOGI
#define LOG_W ALOGW
#define LOG_E ALOGE
#define LOG_F ALOGE
#endif

#endif /* BASE_INCLUDE_LOG_LOG_H */
