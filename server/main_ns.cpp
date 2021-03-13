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
#include <common_base/CFdbContext.h>
#include <common_base/fdb_option_parser.h>
#include "CNameServer.h"
#include <iostream>

int main(int argc, char **argv)
{
    char *tcp_addr = 0;
    char *host_name = 0;
    char *interface_ips = 0;
    char *interface_names = 0;
    int32_t help = 0;
    int32_t ret = 0;
    char *watchdog_params = 0;
    const struct fdb_option core_options[] = {
        { FDB_OPTION_STRING, "url", 'u', &tcp_addr },
        { FDB_OPTION_STRING, "name", 'n', &host_name },
        { FDB_OPTION_STRING, "interface ip list", 'i', &interface_ips },
        { FDB_OPTION_STRING, "interface name list", 'm', &interface_names },
        { FDB_OPTION_STRING, "watchdog", 'd', &watchdog_params },
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
        std::cout << "Usage: name_server[ -n host_name][ -u host_url][ -i ip1,ip2...][ -m if_name1,if_name2...]" << std::endl;
        std::cout << "Service naming server" << std::endl;
        std::cout << "    -n host_name: host name of this machine" << std::endl;
        std::cout << "    -u host_url: the URL of host server to be connected" << std::endl;
        std::cout << "    -i ip1,ip2...: interfaces to listen on in form of IP address" << std::endl;
        std::cout << "    -m if_name1,if_name2...: interfaces to listen on in form of interface name" << std::endl;
        std::cout << "    -d interval:retries: enable watchdog and specify interval between feeding dog in ms (interval)" << std::endl;
        std::cout << "         and maximum number of retries (retries). If '0' is given, default value will be used." << std::endl;
        return 0;
    }

    uint32_t num_interface_ips = 0;
    char **interface_ips_array = interface_ips ? strsplit(interface_ips, ",", &num_interface_ips) : 0;
    uint32_t num_interface_names = 0;
    char **interface_names_array = interface_names ? strsplit(interface_names, ",", &num_interface_names) : 0;

    int32_t wd_interval = 0;
    int32_t wd_retries = 0;
    uint32_t num_wd_params = 0;
    char **wd_params_array = watchdog_params ? strsplit(watchdog_params, ":", &num_wd_params) : 0;
    bool watchdog_enabled = false;
    if (wd_params_array)
    {
        if (num_wd_params != 2)
        {
            std::cout << "Bad watchdog parameters! please use -d interval:retries." << std::endl;
            return -1;
        }
        watchdog_enabled = true;
        wd_interval = atoi(wd_params_array[0]);
        wd_retries = atoi(wd_params_array[1]);
    }

    FDB_CONTEXT->enableNameProxy(false);
    FDB_CONTEXT->enableLogger(false);
    FDB_CONTEXT->init();
    CNameServer *ns = new CNameServer();
    if (watchdog_enabled)
    {
        std::cout << "Starting watchdog with interval " << wd_interval << " and retries " << wd_retries << std::endl;
        ns->startWatchdog(wd_interval, wd_retries);
    }
    if (!ns->online(tcp_addr, host_name, interface_ips_array, num_interface_ips,
                    interface_names_array, num_interface_names))
    {
        ret = -1;
        goto _quit;
    }

    FDB_CONTEXT->start(FDB_WORKER_EXE_IN_PLACE);
_quit:
    if (tcp_addr)
    {
        free(tcp_addr);
    }
    if (host_name)
    {
        free(host_name);
    }
    return ret;
}
