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

#include <algorithm>
#include <iostream>  
#include <stdlib.h>  
#include <stdio.h>  
#include <string.h>  
#ifdef __WIN32__
#include <direct.h>  
#include <io.h>  
#else
#include <unistd.h>  
#include <dirent.h>  
#endif
#include <sys/stat.h>
#include "CLogFileManager.h"
#include <common_base/CBaseSysDep.h>

#define FDB_LOG_FILE_INDEX_SIZE 8

#define _fdb_filename_format(_size) "%0" #_size "d-%s-%s.txt"
#define fdb_filename_format(_size) _fdb_filename_format(_size)

#ifdef __WIN32__
#define FDB_PATH_SEPARATOR "\\"
#else
#define FDB_PATH_SEPARATOR "/"
#endif

const char *CLogFileManager::mDefaultBaseName = "fdbus";

CLogFileManager::CLogFileManager(const char *log_path,
                                 const char *base_name,
                                 int64_t max_total_size,
                                 int64_t max_file_size)
    : mLogPath(log_path ? log_path : "")
    , mBaseName(base_name ? base_name : mDefaultBaseName)
    , mMaxStorageSize(max_total_size ? max_total_size : mDefaultMaxStorageSize)
    , mMaxFileSize(max_file_size ? max_file_size : mDefaultMaxFileSize)
    , mCurrentStorageSize(0)
    , mCurrentFp(0)
    , mFileId(-1)
    , mEnableBuffer(false)
    , mBuffer(0)
    , mBufferSize(0)
{
    if (!mLogPath.empty() && (mMaxStorageSize > mMaxFileSize))
    {
        scanDir();
    }
}

CLogFileManager::~CLogFileManager()
{
    if (mCurrentFp)
    {
        fclose(mCurrentFp);
    }
}

bool CLogFileManager::setWorkingPath(const char *path)
{
    if (path && (path[0] != '\0') && (mMaxStorageSize > mMaxFileSize) && mLogPath.compare(path))
    {
        if (mCurrentFp)
        {
            fclose(mCurrentFp);
            mCurrentFp = 0;
        }
        mCurrentStorageSize = 0;
        mFilePool.clear();
        mLogPath = path;
        return scanDir();
    }
    return false;
}

bool CLogFileManager::setStorageSize(int64_t max_storage_size, int64_t max_file_size)
{
    if (max_file_size <= max_storage_size)
    {
        mMaxStorageSize = max_storage_size;
        mMaxFileSize = max_file_size;
        return true;
    }
    return false;
}

bool CLogFileManager::scanDir()
{
    if (mLogPath.empty())
    {
        return false;
    }
#ifdef __WIN32__
    _finddata_t file;
    intptr_t lf;
    std::string file_path = mLogPath + FDB_PATH_SEPARATOR "*.*";
    if ((lf = _findfirst(file_path.c_str(), &file)) == -1)
    {
        std::cout << "CLogFileManager: Error! unable to access directory "
                  << mLogPath.c_str() << std::endl;
        return false;
    }
    while(_findnext(lf, &file) == 0)
    {  
        addFile(file.name);
    }

    _findclose(lf);
#else
    DIR *dir;
    struct dirent *ptr;

    if ((dir=opendir(mLogPath.c_str())) == NULL)
    {
        std::cout << "CLogFileManager: Error! unable to access directory "
                  << mLogPath.c_str() << std::endl;
        return false;
    }  

    while ((ptr=readdir(dir)) != NULL)
    {
        if(ptr->d_type == DT_REG)
        {
            addFile(ptr->d_name);
        }
    }
    closedir(dir);
#endif

    std::sort(mFilePool.begin(), mFilePool.end(), CLogFileManager::compareAsc);
    mFileId++;
    return true;
}

bool CLogFileManager::addFile(const char *file_name)
{
    if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0)
    {
        return false;
    }

    struct stat statbuf;
    std::string abs_path;
    getAbsPath(abs_path, file_name);
    if (stat(abs_path.c_str(), &statbuf) < 0)
    {
        return false;
    }

    auto file_size = (int64_t)statbuf.st_size;
    mCurrentStorageSize += file_size;
    
    int32_t index = getFileIndex(file_name);
    if (index < 0)
    {
        return false;
    }
    if (mFileId < index)
    {
        mFileId = index;
    }

    mFilePool.push_back(CFileInfo(file_name, file_size));
    return true;
}

int32_t CLogFileManager::getFileIndex(const char *file_name)
{
    std::string std_file = file_name;
    if (std_file.size() <= FDB_LOG_FILE_INDEX_SIZE)
    {
        return -1;
    }
    if (std_file[FDB_LOG_FILE_INDEX_SIZE] != '-')
    {
        return -1;
    }

    std::string str_index = std_file.substr(0, FDB_LOG_FILE_INDEX_SIZE);
    int32_t index;
    try
    {
        index = std::stoi(str_index);
    }
    catch (...)
    {
        index = -1;
    }

    return index;
}

void CLogFileManager::getAbsPath(std::string &abs_path, const char *relative_name)
{
    abs_path = mLogPath + FDB_PATH_SEPARATOR;
    if (relative_name)
    {
        abs_path += relative_name;
    }
    else
    {
        abs_path += mCurrentFileName;
    }
}

void CLogFileManager::nextFileName()
{
    char name_buf[1024];
    char date_buf[64];
    sysdep_gettimestamp(date_buf, sizeof(date_buf), 0, 1);
    date_buf[63] = '\0';
    snprintf(name_buf, sizeof(name_buf), fdb_filename_format(FDB_LOG_FILE_INDEX_SIZE),
             mFileId++, mBaseName.c_str(), date_buf);
    mCurrentFileName = name_buf;
}

void CLogFileManager::checkFileSize()
{
    if (mCurrentFp)
    {
        int64_t file_size = (int64_t)ftell(mCurrentFp);
        if (file_size >= mMaxFileSize)
        {
            fclose(mCurrentFp);
            mCurrentFp = 0;
            mFilePool.push_back(CFileInfo(mCurrentFileName.c_str(), file_size));
            mCurrentStorageSize += file_size;
        }
    }

    if (!mCurrentFp)
    {
        while ((mCurrentStorageSize + mMaxFileSize) > mMaxStorageSize)
        {
            if (mFilePool.empty())
            {
                mCurrentStorageSize = 0;
                break;
            }

            auto &file_info = mFilePool.front();
            mCurrentStorageSize -= file_info.mFileSize;
            std::string abs_path;
            getAbsPath(abs_path, file_info.mFileName.c_str());
            std::remove(abs_path.c_str());
            mFilePool.pop_front();

            if (mCurrentStorageSize < 0)
            {
                mCurrentStorageSize = 0;
                break;
                // TODO: check if mFilePool is empty
            }
        }
        
        while (!mCurrentFp)
        {
            nextFileName();
            std::string current_file;
            getAbsPath(current_file);
            mCurrentFp = fopen(current_file.c_str(), "wb");
        }

        if (mCurrentFp)
        {
            if (mEnableBuffer)
            {
                if (mBuffer && mBufferSize)
                {
                    setvbuf(mCurrentFp, mBuffer, _IOFBF, mBufferSize);
                }
            }
            else
            {
                setbuf(mCurrentFp, 0);
            }
        }
    }
}

bool CLogFileManager::store(const std::string &ostring)
{
    if ((mFileId < 0) || mLogPath.empty() || ostring.empty())
    {
        return false;
    }
    checkFileSize();
    if (mCurrentFp)
    {
        auto size = ostring.size();
        auto n = fwrite(ostring.c_str(), 1, size, mCurrentFp);
        return size == n;
    }
    return false;
}

bool CLogFileManager::compareAsc(const CLogFileManager::CFileInfo &info1, const CLogFileManager::CFileInfo &info2)
{
    return info1.mFileName < info2.mFileName;
}
