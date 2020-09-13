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

#ifndef _CBASESERVER_H_
#define _CBASESERVER_H_

#include "common_defs.h"
#include "CBaseFdWatch.h"
#include "CFdbSessionContainer.h"
#include "CBaseEndpoint.h"
#include "CMethodJob.h"
#include "CApiSecurityConfig.h"

class CBaseServer;
class CServerSocket : public CBaseFdWatch, public CFdbSessionContainer
{
public:
    CServerSocket(CBaseServer *owner
                  , FdbSocketId_t skid
                  , CServerSocketImp *socket);
    ~CServerSocket();
    bool bind(CBaseWorker *worker);
    void unbind();
    void getSocketInfo(CFdbSocketInfo &info);
protected:
    void onInput(bool &io_error);
private:
    CServerSocketImp *mSocket;
};

class CBaseServer : public CBaseEndpoint
{
public:
    CBaseServer(const char *name, CBaseWorker *worker = 0);
    virtual ~CBaseServer();

    /*
     * Bind the server with an address
     * @iparam server: the server binding the specified address
     * @iparam address: the address to be connected; if not specified,
     *     use "svc://server_name", where server_name is the string passed
     *     when creating CBaseServer.
     * @return: the socket the server is listening to. To check the return
     *     value, using isValidFdbId();
     *     Note that for "svc://", isValidFdbId() always return false
     *
     * The supported address format is:
     * tcp://ip address:port number
     * ipc://directory to unix domain socket
     * svc://server name: own server name and get address dynamically
     *     allocated by name server
     *
     * Multiple address can be bounded to the same server
     */
    FdbSocketId_t bind(const char *url = 0);

    /*
     * Unbind a socket that is already bound
     * @iparam skid: the socket return from bind()
     */
    void unbind(FdbSocketId_t skid = FDB_INVALID_ID);

    void prepareDestroy();

protected:
    void onSidebandInvoke(CBaseJob::Ptr &msg_ref);
    bool onMessageAuthentication(CFdbMessage *msg, CFdbSession *session);
    bool onEventAuthentication(CFdbMessage *msg, CFdbSession *session);

    bool publishNoQueue(FdbMsgCode_t code, const char *topic, const void *buffer,
                        int32_t size, CFdbSession *session);
    void publishCachedEvents(CFdbSession *session);
private:
    CApiSecurityConfig mApiSecurity;
    void cbBind(CBaseWorker *worker, CMethodJob<CBaseServer> *job, CBaseJob::Ptr &ref);
    CServerSocket *doBind(const char *url);

    void cbUnbind(CBaseWorker *worker, CMethodJob<CBaseServer> *job, CBaseJob::Ptr &ref);
    void doUnbind(FdbSocketId_t skid = FDB_INVALID_ID);
    
    friend class CFdbContext;
    friend class CServerSocket;
    friend class CIntraNameProxy;
    friend class CNameServer;
};

#endif
