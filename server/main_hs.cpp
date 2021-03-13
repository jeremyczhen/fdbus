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

#include <common_base/CFdbContext.h>
#include "CHostServer.h"
#include <common_base/fdb_option_parser.h>
#include <iostream>

int main(int argc, char **argv)
{
    int32_t help = 0;
	const struct fdb_option core_options[] = {
        { FDB_OPTION_BOOLEAN, "help", 'h', &help }
    };

	fdb_parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);
    if (help)
    {
        std::cout << "FDBus - Fast Distributed Bus" << std::endl;
        std::cout << "    SDK version " << FDB_DEF_TO_STR(FDB_VERSION_MAJOR) "."
                                           FDB_DEF_TO_STR(FDB_VERSION_MINOR) "."
                                           FDB_DEF_TO_STR(FDB_VERSION_BUILD) << std::endl;
        std::cout << "    LIB version " << CFdbContext::getFdbLibVersion() << std::endl;
        std::cout << "Usage: host_server" << std::endl;
        std::cout << "    Start host server in case of multi-host." << std::endl;
        std::cout << "    Note that only one instance is needed to run for a multi-domain system." << std::endl;
        return 0;
    }

    FDB_CONTEXT->enableLogger(false);
    FDB_CONTEXT->init();
    CHostServer *hs = new CHostServer();
    if (!hs)
    {
        return 0;
    }

    hs->bind();
    FDB_CONTEXT->start(FDB_WORKER_EXE_IN_PLACE);
    return 0;
}
