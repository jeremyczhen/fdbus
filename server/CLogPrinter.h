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

#ifndef __CLOGPRINTER_H__
#define __CLOGPRINTER_H__
#include <string>
#include <common_base/CFdbMessage.h>

#define LOG_MODE_STDOUT         (1 << 0)
#define LOG_MODE_FILE           (1 << 1)

class CFdbSimpleDeserializer;
class CLogPrinter
{
public:
    CLogPrinter(uint32_t mode = LOG_MODE_STDOUT);
    void outputFdbLog(CFdbSimpleDeserializer &log_info, CFdbMessage *log_msg);
    void outputTraceLog(CFdbSimpleDeserializer &trace_info, CFdbMessage *trace_msg);
    void setPrintMode(uint32_t print_mode)
    {
        mPrintMode = print_mode;
    }
    void setLogFile(const char *log_file)
    {
        if (log_file)
        {
            mLogFile = log_file;
        }
    }
private:
    uint32_t mPrintMode;
    std::string mLogFile;
};
#endif
