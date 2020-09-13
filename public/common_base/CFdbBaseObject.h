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

#ifndef _CFDBBASEOBJECT_H_
#define _CFDBBASEOBJECT_H_

#include <map>
#include <set>
#include "CFdbMessage.h"
#include "CMethodJob.h"
#include "CFdbMsgSubscribe.h"

enum EFdbEndpointRole
{
    FDB_OBJECT_ROLE_SERVER,
    FDB_OBJECT_ROLE_NS_SERVER,
    FDB_OBJECT_ROLE_CLIENT,
    FDB_OBJECT_ROLE_UNKNOWN
};

class CBaseWorker;
class CFdbSession;
class CBaseEndpoint;
class IFdbMsgBuilder;
struct CFdbSessionInfo;

typedef CFdbMsgTable CFdbMsgSubscribeList;
typedef CFdbMsgTable CFdbMsgTriggerList;

typedef std::set<std::string> tFdbFilterSets;
typedef std::map<FdbMsgCode_t, tFdbFilterSets> tFdbSubscribeMsgTbl;
typedef std::set<CFdbSession *> tSubscribedSessionSets;
class CFdbBaseObject
{
public:
#define FDB_OBJ_ENABLED_MIGRATE         (1 << 0)
#define FDB_OBJ_AUTO_REMOVE             (1 << 1)
#define FDB_OBJ_RECONNECT_ENABLED       (1 << 2)
#define FDB_OBJ_REGISTERED              (1 << 3)
#define FDB_OBJ_RECONNECT_ACTIVATED     (1 << 4)
#define FDB_OBJ_ENABLE_EVENT_CACHE      (1 << 5)

    CFdbBaseObject(const char *name = 0, CBaseWorker *worker = 0, EFdbEndpointRole role = FDB_OBJECT_ROLE_UNKNOWN);
    virtual ~CFdbBaseObject();

    /*
     * invoke[1]
     * Invoke method call at receiver asynchronously. It can be called from client
     * to server or from server to client.
     * @iparam receiver: the receiver the request is sent to
     * @iparam code: the message code
     * @iparam data: the message data sent to receiver
     * @iparam timeout: timeout in unit of ms; 0 if no timer is specified
     *
     * CBaseServer::onInvoke() or CBaseClient::onInvoke() will be called to
     * process the request and reply will be sent after the request is
     * processed, which trigger invokation of onResponse() of the message;
     * If timeout is specified but receiver doesn't reply within specified time,
     * status is returned with error code being FDB_ST_TIMEOUT.
     */
    bool invoke(FdbSessionId_t receiver
                , FdbMsgCode_t code
                , IFdbMsgBuilder &data
                , int32_t timeout = 0);

    /*
     * invoke[1.1]
     * Similiar to invoke[1] but instead of passing message code, message
     * object is given. Usually the object is subclass of CBaseMessage so
     * that extra data can be attached.
     * In onInvoke(CBaseJob::Ptr &msg_ref), castToMessage() can be used to
     * convert msg_ref to subclass of CBaseMessage so that the extra data
     * can be retrieved.
     */
    bool invoke(FdbSessionId_t receiver
                , CFdbMessage *msg
                , IFdbMsgBuilder &data
                , int32_t timeout = 0);

    /*
     * invoke[2]
     * Similiar to invoke[1] without parameter 'receiver'. The method is called at
     * CBaseClient to invoke method upon connected server.
     */
    bool invoke(FdbMsgCode_t code
                , IFdbMsgBuilder &data
                , int32_t timeout = 0);

    /*
     * invoke[2.1]
     * Similiar to invoke[1.1] without parameter 'receiver'. The method is called at
     * CBaseClient to invoke method upon connected server.
     */
    bool invoke(CFdbMessage *msg
                , IFdbMsgBuilder &data
                , int32_t timeout = 0);

    /*
     * invoke[3]
     * Similiar to invoke[1] but is synchronous, which means the method blocks
     * until receiver reply. The added parameter 'msg_ref' is used to pass
     * (subclass of) CBaseMessage object in when called, and pass the same object
     * out, in which return data can be retrieved.
     *
     * @ioparam msg_ref: input - object of CBaseMessage or its subclass
     *                   output - the same object containing return data
     */
    bool invoke(FdbSessionId_t receiver
                , CBaseJob::Ptr &msg_ref
                , IFdbMsgBuilder &data
                , int32_t timeout = 0);

    /*
     * invoke[4]
     * Similiar to invoke[3] but 'receiver' is not specified. The method is
     * called by CBaseClient to invoke method upon connected server.
     */
    bool invoke(CBaseJob::Ptr &msg_ref
                , IFdbMsgBuilder &data
                , int32_t timeout = 0);

    /*
     * invoke[5]
     * Similiar to invoke[1] but raw data is sent rather than proto-buffer.
     * @iparam buffer: buffer holding raw data
     * @iparam size: size of buffer
     */
    bool invoke(FdbSessionId_t receiver
                , FdbMsgCode_t code
                , const void *buffer = 0
                , int32_t size = 0
                , int32_t timeout = 0
                , const char *log_info = 0);

    bool invoke(FdbSessionId_t receiver
                , CFdbMessage *msg
                , const void *buffer = 0
                , int32_t size = 0
                , int32_t timeout = 0);

    /*
     * invoke[6]
     * Similiar to invoke[2] but raw data is sent rather than proto-buffer.
     * @iparam buffer: buffer holding raw data
     * @iparam size: size of buffer
     */
    bool invoke(FdbMsgCode_t code
                , const void *buffer = 0
                , int32_t size = 0
                , int32_t timeout = 0
                , const char *log_data = 0);

    bool invoke(CFdbMessage *msg
                , const void *buffer = 0
                , int32_t size = 0
                , int32_t timeout = 0);

    /*
     * invoke[7]
     * Similiar to invoke[3] but raw data is sent rather than proto-buffer.
     * @iparam buffer: buffer holding raw data
     * @iparam size: size of buffer
     */
    bool invoke(FdbSessionId_t receiver
                , CBaseJob::Ptr &msg_ref
                , const void *buffer = 0
                , int32_t size = 0
                , int32_t timeout = 0);

    /*
     * invoke[8]
     * Similiar to invoke[4] but raw data is sent rather than proto-buffer.
     * @iparam buffer: buffer holding raw data
     * @iparam size: size of buffer
     */
    bool invoke(CBaseJob::Ptr &msg_ref
                , const void *buffer = 0
                , int32_t size = 0
                , int32_t timeout = 0);

    /*
     * send[1]
     * send protocol buffer to receiver. No reply is expected; just one-shot
     * @iparam receiver: the receiver to get the message
     * @iparam code: message code
     * @imaram data: message in protocol buffer format
     */
    bool send(FdbSessionId_t receiver
              , FdbMsgCode_t code
              , IFdbMsgBuilder &data);
              
    /*
     * send[2]
     * Similiar to send[1] without parameter 'receiver'. The method is called
     * from CBaseClient to send message to the connected server.
     */
    bool send(FdbMsgCode_t code, IFdbMsgBuilder &data);

    /*
     * send[3]
     * Similiar to send[1] but raw data is sent.
     * @iparam buffer: buffer holding raw data
     * @iparam size: size of buffer
     */
    bool send(FdbSessionId_t receiver
              , FdbMsgCode_t code
              , const void *buffer = 0
              , int32_t size = 0
              , const char *log_data = 0);
    /*
     * send[4]
     * Similiar to send[2] but raw data is sent.
     * @iparam buffer: buffer holding raw data
     * @iparam size: size of buffer
     */
    bool send(FdbMsgCode_t code
              , const void *buffer = 0
              , int32_t size = 0
              , const char *log_data = 0);

    /*
     * get[1]
     * Get current value of event code/topic pair asynchronously
     * The value is returned in the same way as invoke(), i.e., from onReply()
     */
    bool get(FdbMsgCode_t code, const char *topic = 0, int32_t timeout = 0);

    /*
     * get[2]
     * The same as get[1] except that the given 'msg' can convery extra user
     *     that can be retrieved from onReply()
     * The value is returned in the same way as invoke(), i.e., from onReply()
     */
    bool get(CFdbMessage *msg, const char *topic = 0, int32_t timeout = 0);

    /*
     * get[3]
     * Get current value of event code/topic pair synchronously 
     * Once return, the value is containd in msg_ref
     */
    bool get(CBaseJob::Ptr &msg_ref, const char *topic = 0, int32_t timeout = 0);

    /*
     * broadcast[1]
     * broadcast message to all connected clients.
     * The clients should have already registered message 'code' with
     * optional filter.
     * @iparam code: code of the message to be broadcasted
     * @iparam data: message of protocol buffer
     * @iparam filter: the filter associated with the broadcasting
     */
    bool broadcast(FdbMsgCode_t code
                   , IFdbMsgBuilder &data
                   , const char *filter = 0);
    /*
     * broadcast[3]
     * Similiar to broadcast[1] except that raw data is broadcasted.
     * @iparam buffer: buffer holding raw data
     * @iparam size: size of buffer
     */
    bool broadcast(FdbMsgCode_t code
                   , const void *buffer = 0
                   , int32_t size = 0
                   , const char *filter = 0
                   , const char *log_data = 0);
    /*
     * Build subscribe list before calling subscribe().
     * The event added is updated by brocast() from server or update()
     * from client.
     *
     * @oparam msg_list: the list holding message sending subscribe
     *      request to server
     * @iparam msg_code: The message code to subscribe
     * @iparam filter: the filter associated with the message.
     */
    static void addNotifyItem(CFdbMsgSubscribeList &msg_list
                              , FdbMsgCode_t msg_code
                              , const char *filter = 0);

    /*
     * Build subscribe list before calling subscribe().
     * Instead of specific event, the whole event group is subscribed.
     * When broadcast() at server is called to broadcast an event, the
     * the clients subscribed the event group that the event belongs
     * to is notified (onBroadcast() is called).
     *
     * @oparam msg_list: the list holding message sending subscribe
     *      request to server
     * @iparam event_group: The event group to subscribe
     * @iparam filter: the filter associated with the message.
     */
    static void addNotifyGroup(CFdbMsgSubscribeList &msg_list
                              , FdbEventGroup_t event_group = FDB_DEFAULT_GROUP
                              , const char *filter = 0);

    /*
     * Build subscribe list for update on request before calling subscribe().
     * Unlike addNotifyItem(), the event added can only be updated by update()
     * from client.
     *
     * @oparam msg_list: the list holding message sending subscribe
     *      request to server
     * @iparam msg_code: The message code to subscribe
     * @iparam filter: the filter associated with the message.
     */
    static void addUpdateItem(CFdbMsgSubscribeList &msg_list
                              , FdbMsgCode_t msg_code
                              , const char *filter = 0);

    /*
     * Build subscribe list for update on request before calling subscribe().
     * Unlike addNotifyGroup(), the event group added can only be updated by
     * update() from client.
     *
     * @oparam msg_list: the list holding message sending subscribe
     *      request to server
     * @iparam event_group: The event group to subscribe
     * @iparam filter: the filter associated with the message.
     */
    static void addUpdateGroup(CFdbMsgSubscribeList &msg_list
                              , FdbEventGroup_t event_group = FDB_DEFAULT_GROUP
                              , const char *filter = 0);

    /*
     * Build update list to trigger update manually.
     *
     * @oparam msg_list: the list holding message sending update
     *      trigger to server
     * @iparam msg_code: The message code to trigger
     * @iparam filter: the filter associated with the message.
     */
    static void addTriggerItem(CFdbMsgTriggerList &msg_list
                                , FdbMsgCode_t msg_code
                                , const char *filter = 0);

    /*
     * Build update list to trigger update manually.
     * All events in the group will be updated.
     *
     * @oparam msg_list: the list holding message sending update
     *      trigger to server
     * @iparam event_group: The event group to trigger
     * @iparam filter: the filter associated with the message.
     */
    static void addTriggerGroup(CFdbMsgTriggerList &msg_list
                                , FdbEventGroup_t event_group = FDB_DEFAULT_GROUP
                                , const char *filter = 0);
    /*
     * subscribe[1]
     * Subscribe a list of messages so that the messages can be received
     *      when the server broadcast a message.
     * The method doesn't block and is asynchronous. Once completed, onStatus()
     *      will be called with CBaseMessage::isSubscribe() being true.
     *
     *   server                                  client
     *     |                            /-----------|
     *     |                addNotifyItem(list,...) |
     *     |                            \---------->|
     *     |                            /-----------|
     *     |                addUpdateItem(list,...) |
     *     |                            \---------->|
     *     |<------------subscribe(list,...)--------|
     *     |---CFdbMessage::broadcast(event1)------>|
     *     |                            /-----------|
     *     |                       onBroadcast()    |
     *     |                            \---------->|
     *     |---CFdbMessage::broadcast(event2)------>|
     *     |                            /-----------|
     *     |                       onBroadcast()    |
     *     |                            \---------->|
     *     |------status()[auto reply]------------->|
     *     |                    /-------------------|
     *     |     onStatus()[isSubscribe()==true]    |
     *     |                    \------------------>|
     *     |                                        |
     *     |---CBaseServer::broadcast(event1)------>|
     *     |                            /-----------|
     *     |                       onBroadcast()    |
     *     |                            \---------->|
     *     |                                        |
     *
     * @iparam msg_list: list of messages to be subscribed
     * @iparam timeout: optional timeout
     */
    bool subscribe(CFdbMsgSubscribeList &msg_list
                   , int32_t timeout = 0);

    /*
     * subscribe[1.1]
     * Similiar to subscribe[1] but additional parameter msg is given. The
     *      msg is object of subclass of CBaseMessage allowing extra data to be
     *      attached with the transaction.
     * in onStatus() with CBaseMessage::isSubscribe() being true, the object
     *      can be retrieved with castToMessage().
     *
     * @iparam msg_ist: list of messages to be retrieved
     * @iparam msg: (subclass of) CBaseMessage containing transaction-
     *      specific data
     * @iparam timeout: a timer expires if response is not received from
     *      server with the specified time.
     */
    bool subscribe(CFdbMsgSubscribeList &msg_list
                   , CFdbMessage *msg
                   , int32_t timeout = 0);

    /*
     * subscribeSync
     * Similiar to subscribe[1] but is synchronous. It will block until the
     *      server has registered all messages in list.
     *     A key feature of subscribeSync() is that it can guarantee all
     *     onBroadcast() is called before returning. This means all events are
     *     initialized once return from the call.
     *
     * @iparam msg_ist: list of messages to be retrieved
     * @iparam timeout: timer of the transaction
     */
    bool subscribeSync(CFdbMsgSubscribeList &msg_list
                       , int32_t timeout = 0);

    /*
     * update[1]
     * Request to update a list of messages manually. In essence it takes
     *      advantage of subscribe-broadcast to trigger a broadcast upon
     *      selected events.
     * The method doesn't block and is asynchronous. Once completed, onStatus()
     *      will be called with CBaseMessage::isSubscribe() being true.
     * Note that the events to be updated should be subscribed with subscribe()
     *      or can not be updated.
     *
     *   server                                  client
     *     |                                        |
     *     |   The same sequence as subscribe()[1]  |
     *     |                                        |
     *     |                            /-----------|
     *     |             addTriggerItem(list,...)   |
     *     |                            \---------->|
     *     |<------------update(list,...)-----------|
     *     |----CBaseMessage::broadcast(event1)---->|
     *     |                            /-----------|
     *     |                       onBroadcast()    |
     *     |                            \---------->|
     *     |----CBaseMessage::broadcast(event2)---->|
     *     |                            /-----------|
     *     |                       onBroadcast()    |
     *     |                            \---------->|
     *     |------status()[auto reply]------------->|
     *     |                    /-------------------|
     *     |     onStatus()[isSubscribe()==true]    |
     *     |                    \------------------>|
     *
     * @iparam msg_list: list of messages to be subscribed
     * @iparam timeout: optional timeout
     */
    bool update(CFdbMsgTriggerList &msg_list, int32_t timeout = 0);

    /*
     * update[1.1]
     * Similiar to update[1] but additional parameter msg is given. The
     *      msg is object of subclass of CBaseMessage allowing extra data to be
     *      attached with the transaction.
     * in onStatus() with CBaseMessage::isSubscribe() being true, the object
     *      can be retrieved with castToMessage().
     *
     * @iparam msg_ist: list of messages to be retrieved
     * @iparam msg: (subclass of) CBaseMessage containing transaction-
     *      specific data
     * @iparam timeout: a timer expires if response is not received from
     *      server with the specified time.
     */
    bool update(CFdbMsgTriggerList &msg_list, CFdbMessage *msg, int32_t timeout = 0);

    /*
     * updateSync
     * Similiar to update but is synchronous. It will block until the
     *      server has updated all messages in list.
     *     A key feature of updateSync() is that it can guarantee all
     *     onBroadcast() is called before returning. This means all events are
     *     initialized once return from the call.
     *
     * @iparam msg_ist: list of messages to be retrieved
     * @iparam timeout: timer of the transaction
     */
    bool updateSync(CFdbMsgTriggerList &msg_list, int32_t timeout = 0);

    /*
     * Unsubscribe messages listed in msg_list
     */
    bool unsubscribe(CFdbMsgSubscribeList &msg_list);
    
    /*
     * Unsubscribe the whole object
     */
    bool unsubscribe();

    FdbObjectId_t bind(CBaseEndpoint *endpoint, FdbObjectId_t oid = FDB_INVALID_ID);
    FdbObjectId_t connect(CBaseEndpoint *endpoint, FdbObjectId_t oid = FDB_INVALID_ID);

    void unbind();
    void disconnect();

    const std::string &name() const
    {
        return mName;
    }

    /*
     * Get a list of subscribed messages.
     * Warning: not thread-safe!!! MUST be used within CONTEXT!!!
     */
    void getSubscribeTable(tFdbSubscribeMsgTbl &table);
    void getSubscribeTable(FdbMsgCode_t code, tFdbFilterSets &filters);
    void getSubscribeTable(FdbMsgCode_t code, const char *filter,
                            tSubscribedSessionSets &session_tbl);

    CBaseWorker *worker() const
    {
        return mWorker;
    }

    // Warning: Not thread safe and should be set before working!!
    void worker(CBaseWorker *worker)
    {
        mWorker = worker;
    }

    FdbObjectId_t objId() const
    {
        return mObjId;
    }

    EFdbEndpointRole role() const
    {
        return mRole;
    }

    FdbEndpointId_t epid() const;

    bool enableMigrate() const
    {
        return !!(mFlag & FDB_OBJ_ENABLED_MIGRATE);
    }

    void enableMigrate(bool enb)
    {
        if (enb)
        {
            mFlag |= FDB_OBJ_ENABLED_MIGRATE;
        }
        else
        {
            mFlag &= ~FDB_OBJ_ENABLED_MIGRATE;
        }
    }

    void autoRemove(bool enb)
    {
        if (enb)
        {
            mFlag |= FDB_OBJ_AUTO_REMOVE;
        }
        else
        {
            mFlag &= ~FDB_OBJ_AUTO_REMOVE;
        }
    }

    bool autoRemove() const
    {
        return !!(mFlag & FDB_OBJ_AUTO_REMOVE);
    }
    
    void enableReconnect(bool active)
    {
        if (active)
        {
            mFlag |= FDB_OBJ_RECONNECT_ENABLED;
        }
        else
        {
            mFlag &= ~FDB_OBJ_RECONNECT_ENABLED;
        }
    }

    bool reconnectEnabled() const
    {
        return !!(mFlag & FDB_OBJ_RECONNECT_ENABLED);
    }

    void activateReconnect(bool active)
    {
        if (active)
        {
            mFlag |= FDB_OBJ_RECONNECT_ACTIVATED;
        }
        else
        {
            mFlag &= ~FDB_OBJ_RECONNECT_ACTIVATED;
        }
    }

    bool reconnectActivated() const
    {
        return !!(mFlag & FDB_OBJ_RECONNECT_ACTIVATED);
    }

    void enableEventCache(bool active)
    {
        if (active)
        {
            mFlag |= FDB_OBJ_ENABLE_EVENT_CACHE;
        }
        else
        {
            mFlag &= ~FDB_OBJ_ENABLE_EVENT_CACHE;
        }
    }

    bool enableEventCache() const
    {
        return !!(mFlag & FDB_OBJ_ENABLE_EVENT_CACHE);
    }

    void setDefaultSession(FdbSessionId_t sid = FDB_INVALID_ID)
    {
        mSid = sid;
    }

    FdbSessionId_t getDefaultSession()
    {
        return mSid;
    }

    void initEventCache(FdbMsgCode_t event
                        , const char *topic
                        , IFdbMsgBuilder &data
                        , bool always_update = false);

    void initEventCache(FdbMsgCode_t event
                        , const char *topic
                        , const void *buffer
                        , int32_t size
                        , bool always_update = false);

    // Internal use only!!!
    bool broadcast(FdbSessionId_t sid
                  , FdbObjectId_t obj_id
                  , FdbMsgCode_t code
                  , IFdbMsgBuilder &data
                  , const char *filter = 0);

    // Internal use only!!!
    bool broadcast(FdbSessionId_t sid
                  , FdbObjectId_t obj_id
                  , FdbMsgCode_t code
                  , const void *buffer = 0
                  , int32_t size = 0
                  , const char *filter = 0
                  , const char *log_data = 0);

    // Internal use only!!!
    bool broadcast(CFdbMessage *msg, CFdbSession *session);

    /*
     * request-reply through side band.
     * Note: Only used internally for FDBus!!!
     */
    bool invokeSideband(FdbMsgCode_t code
                      , IFdbMsgBuilder &data
                      , int32_t timeout = 0);
    bool invokeSideband(FdbMsgCode_t code
                      , const void *buffer = 0
                      , int32_t size = 0
                      , int32_t timeout = 0);
    bool sendSideband(FdbMsgCode_t code, IFdbMsgBuilder &data);
    bool sendSideband(FdbMsgCode_t code
                    , const void *buffer = 0
                    , int32_t size = 0);

    virtual void prepareDestroy();
protected:
    std::string mName;
    CBaseEndpoint *mEndpoint; // Which endpoint the object belongs to
    /*
     * Implemented by server: called when client subscribe a message
     *      IMPORTANT: in the callback, current value must sent to the
     *      client represented by sid using unicast version of
     *      broadcast() so that client can be updated asap.
     * @iparam sid - the session subscribing the message
     * @iparam msg_code - the message subscribed
     * @iparam filter - the filter subscribed
     */
    virtual void onSubscribe(CBaseJob::Ptr &msg_ref)
    {}
    /*
     * Implemented by client: called when server broadcast a message.
     * @iparam msg_ref: reference to a message. Using castToMessage()
     *      to convert it to CBaseMessage.
     */
    virtual void onBroadcast(CBaseJob::Ptr &msg_ref)
    {}
    /*
     * Implemented by either client or server: called at receiver when
     *      the sender call invoke() or send() upon the receiver.
     */
    virtual void onInvoke(CBaseJob::Ptr &msg_ref)
    {}

    /*
     * Implemented by either client or server: called when a session
     *      is destroyed.
     * @iparam sid: id of the session being destroyed
     * @iparam is_last: true when it is the last session to be destroyed
     */
    virtual void onOffline(FdbSessionId_t sid, bool is_last)
    {}
    /*
     * Implemented by either client or server: called when a session
     *      is created.
     * @iparam sid: id of the session created
     * @iparam is_first: true when it is the first session to create
     */
    virtual void onOnline(FdbSessionId_t sid, bool is_first)
    {}
    /*
     * Implemented by either client or server: called when
     *      CBaseMessage::reply() is called by the sender
     */
    virtual void onReply(CBaseJob::Ptr &msg_ref)
    {}
    /*
     * Implemented by either client: response to get()
     *      method when event is returned from server
     */
    virtual void onGetEvent(CBaseJob::Ptr &msg_ref)
    {}
    /*
     * Implemented by either client or server: called when
     *      CBaseMessage::status() is called by the sender
     */
    virtual void onStatus(CBaseJob::Ptr &msg_ref
                          , int32_t error_code
                          , const char *description)
    {}

    /*
     * Implemented by either client or server: called when
     *      an message (subscribe, invoke, send...) is
     *      received targeting an object which does not exist.
     *      The callback should create the associated object
     *      according to the message.
     *      =============== Warning!!! ===================
     *      1. It is running ONLY at CONTEXT thread even
     *      though worker thread is specified!!!
     *      2. If some tasks HAS to be done at other threads,
     *      use synchronous job. The synchronous job shall
     *      NOT call any FDBus API since the CONTEXT is
     *      blocked by the synchronous job.
     */
	virtual void onCreateObject(CBaseEndpoint *endpoint, CFdbMessage *msg)
    {}

    void role(EFdbEndpointRole role)
    {
        mRole = role;
    }

    bool isPrimary()
    {
        return mObjId == FDB_OBJECT_MAIN;
    }

    virtual bool authentication(CFdbSessionInfo const &session_info)
    {
        return true;
    }

    void objId(FdbObjectId_t obj_id)
    {
        mObjId = obj_id;
    }

    bool registered()
    {
        return !!(mFlag & FDB_OBJ_REGISTERED);
    }

    void registered(bool active)
    {
        if (active)
        {
            mFlag |= FDB_OBJ_REGISTERED;
        }
        else
        {
            mFlag &= ~FDB_OBJ_REGISTERED;
        }
    }

    virtual void onSidebandInvoke(CBaseJob::Ptr &msg_ref);
    virtual void onSidebandReply(CBaseJob::Ptr &msg_ref)
    {}

    virtual bool onMessageAuthentication(CFdbMessage *msg, CFdbSession *session)
    {
        return true;
    }
    virtual bool onEventAuthentication(CFdbMessage *msg, CFdbSession *session)
    {
        return true;
    }

    // Warning: only used by FDBus internally!!!!!!!
    void broadcastLogNoQueue(FdbMsgCode_t code, const uint8_t *data, int32_t size, const char *filter);
    void broadcastNoQueue(FdbMsgCode_t code, const uint8_t *data, int32_t size, const char *filter, bool force_update);
private:
    struct CSubscribeItem
    {
        CFdbSubscribeType mType;
    };
    typedef std::map<std::string, CSubscribeItem> SubItemTable_t;
    typedef std::map<FdbObjectId_t, SubItemTable_t> ObjectTable_t;
    typedef std::map<CFdbSession *, ObjectTable_t> SessionTable_t;
    typedef std::map<FdbMsgCode_t, SessionTable_t> SubscribeTable_t;

    struct CEventData
    {
        uint8_t *mBuffer;
        int32_t mSize;
        bool mAlwaysUpdate;

        bool setEventCache(const uint8_t *buffer, int32_t size);
        void replaceEventCache(uint8_t *buffer, int32_t size);
        CEventData();
        ~CEventData();
    };
    typedef std::map<std::string, CEventData> CacheDataTable_t;
    typedef std::map<FdbMsgCode_t, CacheDataTable_t> EventCacheTable_t;

    CBaseWorker *mWorker;
    SubscribeTable_t mEventSubscribeTable;
    SubscribeTable_t mGroupSubscribeTable;
    FdbObjectId_t mObjId;
    uint32_t mFlag;
    EFdbEndpointRole mRole;
    FdbSessionId_t mSid;
    EventCacheTable_t mEventCache;

    void subscribe(CFdbSession *session,
                   FdbMsgCode_t msg,
                   FdbObjectId_t obj_id,
                   const char *filter,
                   CFdbSubscribeType type);

    void unsubscribe(CFdbSession *session,
                     FdbMsgCode_t msg,
                     FdbObjectId_t obj_id,
                     const char *filter);
    void unsubscribeSession(SubscribeTable_t &subscribe_table, CFdbSession *session);
    void unsubscribe(CFdbSession *session);
    void unsubscribeObject(SubscribeTable_t &subscribe_table, FdbObjectId_t obj_id);
    void unsubscribe(FdbObjectId_t obj_id);

    bool updateEventCache(CFdbMessage *msg);
    void broadcast(CFdbMessage *msg);
    void broadcast(SubscribeTable_t &subscribe_table, CFdbMessage *msg, FdbMsgCode_t event);
    bool broadcast(SubscribeTable_t &subscribe_table, CFdbMessage *msg, CFdbSession *session, FdbMsgCode_t event);

    bool sendLog(FdbMsgCode_t code, IFdbMsgBuilder &data);
    bool sendLogNoQueue(FdbMsgCode_t code, IFdbMsgBuilder &data);

    void getSubscribeTable(SessionTable_t &sessions, tFdbFilterSets &filters);
    void getSubscribeTable(SubscribeTable_t &subscribe_table, FdbMsgCode_t code, CFdbSession *session,
                           tFdbFilterSets &filter_tbl);
    void getSubscribeTable(FdbMsgCode_t code, CFdbSession *session, tFdbFilterSets &filter_tbl);
    void getSubscribeTable(SubscribeTable_t &subscribe_table, tFdbSubscribeMsgTbl &table);
    void getSubscribeTable(SubscribeTable_t &subscribe_table, FdbMsgCode_t code, tFdbFilterSets &filters);
    void getSubscribeTable(SubscribeTable_t &subscribe_table, FdbMsgCode_t code, const char *filter,
                            tSubscribedSessionSets &session_tbl);
    
    void callOnOffline(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref);
    void callOnOnline(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref);
    void callBindObject(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref);
    void callConnectObject(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref);
    void callUnbindObject(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref);
    void callDisconnectObject(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref);
    void callPrepareDestroy(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref);

    /*
     * The following methods migrate onXXX() callbacks to specified worker thread.
     *      CBaseEndpoint::onXXX() runs at thread CFdbContext if worker is not
     *      specified for a endpoint. You can manually migrate the callbacks to
     *      any worker and run within the context of the worker.
     */
    bool migrateOnSubscribeToWorker(CBaseJob::Ptr &msg_ref, CBaseWorker *worker = 0);
    bool migrateOnBroadcastToWorker(CBaseJob::Ptr &msg_ref, CBaseWorker *worker = 0);
    bool migrateOnInvokeToWorker(CBaseJob::Ptr &msg_ref, CBaseWorker *worker = 0);
    bool migrateOnOfflineToWorker(FdbSessionId_t sid, bool is_last, CBaseWorker *worker = 0);
    bool migrateOnOnlineToWorker(FdbSessionId_t sid, bool is_first, CBaseWorker *worker = 0);
    bool migrateOnReplyToWorker(CBaseJob::Ptr &msg_ref, CBaseWorker *worker = 0);
    bool migrateGetEventToWorker(CBaseJob::Ptr &msg_ref, CBaseWorker *worker = 0);
    bool migrateOnStatusToWorker(CBaseJob::Ptr &msg_ref, CBaseWorker *worker = 0);

    void doSubscribe(CBaseJob::Ptr &msg_ref);
    void doBroadcast(CBaseJob::Ptr &msg_ref);
    void doInvoke(CBaseJob::Ptr &msg_ref);
    void doReply(CBaseJob::Ptr &msg_ref);
    void doGetEvent(CBaseJob::Ptr &msg_ref);
    void doStatus(CBaseJob::Ptr &msg_ref);

    FdbObjectId_t addToEndpoint(CBaseEndpoint *endpoint, FdbObjectId_t obj_id);
    void removeFromEndpoint();
    FdbObjectId_t doBind(CBaseEndpoint *endpoint, FdbObjectId_t obj_id); // Server object
    FdbObjectId_t doConnect(CBaseEndpoint *endpoint, FdbObjectId_t obj_id); // Client object
    void doUnbind();
    void doDisconnect();
    void notifyOnline(CFdbSession *session, bool is_first);
    void notifyOffline(CFdbSession *session, bool is_last);

    void broadcastOneMsg(CFdbSession *session,
                         CFdbMessage *msg,
                         CSubscribeItem &sub_item);
    void broadcastCached(CBaseJob::Ptr &msg_ref);
     
    CBaseEndpoint *endpoint() const
    {
        return mEndpoint;
    }
    const CEventData *getCachedEventData(FdbMsgCode_t msg_code, const char *filter);
    void remoteCallback(CBaseJob::Ptr &msg_ref, long flag);

    friend class COnSubscribeJob;
    friend class COnBroadcastJob;
    friend class COnInvokeJob;
    friend class COnOfflineJob;
    friend class COnOnlineJob;
    friend class COnReplyJob;
    friend class COnGetEventJob;
    friend class COnStatusJob;
    friend class CBindObjectJob;
    friend class CConnectObjectJob;
    friend class CUnbindObjectJob;
    friend class CDisconnectObjectJob;
    friend class CPrepareDestroyJob;

    friend class CBaseClient;
    friend class CBaseServer;
    friend class CFdbSession;
    friend class CFdbMessage;
    friend class CBaseEndpoint;
    friend class CLogProducer;
};

template<typename T>
class CFdbMessageHandle
{
private:
    typedef void (T::*tCallbackFn)(CBaseJob::Ptr &msg_ref);
    typedef std::map<FdbMsgCode_t, tCallbackFn> tCallbackTbl;

public:
    void registerCallback(FdbMsgCode_t code, tCallbackFn callback)
    {
        mCallbackTbl[code] = callback;
    }

    bool processMessage(T *instance, CBaseJob::Ptr &msg_ref)
    {
        CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
        typename tCallbackTbl::iterator it = mCallbackTbl.find(msg->code());
        if (it == mCallbackTbl.end())
        {
            return false;
        }
        (instance->*(it->second))(msg_ref);
        return true;
    }
private:
    tCallbackTbl mCallbackTbl;
};

template<typename T>
class CFdbSubscribeHandle
{
private:
    typedef void (T::*tCallbackFn)(CFdbMessage *msg, const CFdbMsgSubscribeItem *sub_item);
    typedef std::map<FdbMsgCode_t, tCallbackFn> tCallbackTbl;

public:
    void registerCallback(FdbMsgCode_t code, tCallbackFn callback)
    {
        mCallbackTbl[code] = callback;
    }

    bool processMessage(T *instance, CFdbMessage *msg, const CFdbMsgSubscribeItem *sub_item, FdbMsgCode_t code)
    {
        typename tCallbackTbl::iterator it = mCallbackTbl.find(code);
        if (it == mCallbackTbl.end())
        {
            return false;
        }
        (instance->*(it->second))(msg, sub_item);
        return true;
    }
private:
    tCallbackTbl mCallbackTbl;
};

#endif
