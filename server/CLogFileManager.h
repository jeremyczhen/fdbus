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

#ifndef _CLOGFILEMANAGER_H_
#define _CLOGFILEMANAGER_H_

#include <common_base/common_defs.h>
#include <string>
#include <deque>

class CLogFileManager
{
private:
    static const char *mDefaultBaseName;
    static const int64_t mDefaultMaxStorageSize = 64 * 1024 * 1024;
    static const int64_t mDefaultMaxFileSize = 4 * 1024 * 1024;
public:
    CLogFileManager(const char *log_path,
                    const char *base_name = mDefaultBaseName,
                    int64_t max_total_size = mDefaultMaxStorageSize,
                    int64_t max_file_size = mDefaultMaxFileSize);
    virtual ~CLogFileManager();

    //bool store(uint8_t *log_data, int32_t size);
    bool store(const std::string &ostring);

    bool logEnabled() const
    {
        return mFileId >= 0;
    }
    bool setCustomBuffer(char *buffer, uint32_t size)
    {
        mBuffer = buffer;
        mBufferSize = size;
    }
    bool enableBuffering(bool enable);
private:
    struct CFileInfo
    {
        std::string mFileName;
        int64_t mFileSize;
        CFileInfo(const char *file_name, int64_t file_size)
            : mFileName(file_name)
            , mFileSize(file_size)
        {
        }
    };
    typedef std::deque<CFileInfo> tFilePool;

    tFilePool mFilePool;
    std::string mCurrentFileName;
    std::string mLogPath;
    std::string mBaseName;
    int64_t mMaxStorageSize;
    int64_t mMaxFileSize;
    int64_t mCurrentStorageSize;
    FILE *mCurrentFp;
    int32_t mFileId;
    bool mEnableBuffer;
    char *mBuffer;
    uint32_t mBufferSize;

    void checkFileSize();
    bool getLatestFile(std::string &latest_file);
    int32_t getFileIndex(const char *file_name);
    bool addFile(const char *file_name);
    void nextFileName(std::string &file_name);
    void getAbsPath(std::string &abs_path, const char *relative_name = 0);
    void nextFileName();
    static bool compareAsc(const CFileInfo &info1, const CFileInfo &info2);

    bool scanDir();
};

#endif
