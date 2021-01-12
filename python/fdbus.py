"""
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
"""

import sys
import ctypes
import os

fdb_clib = None

def castToChar(wchar):
    if wchar:
        return wchar.encode('utf-8')
    else:
        return wchar

def fdbLogTrace(level, tag, *argv):
    global fdb_clib
    fdb_clib.fdb_log_trace.argtypes = [ctypes.c_int,        #log_level
                                       ctypes.c_char_p,     #tag
                                       ctypes.c_char_p      #data
                                      ]
    log_data = ''
    for i in argv:
        log_data += str(i)
    
    fdb_clib.fdb_log_trace(level, castToChar(tag), castToChar(log_data))

def FDB_LOG_D(tag, *argv):
    fdbLogTrace(1, tag, argv)

def FDB_LOG_I(tag, *argv):
    fdbLogTrace(2, tag, argv)

def FDB_LOG_W(tag, *argv):
    fdbLogTrace(3, tag, argv)

def FDB_LOG_E(tag, *argv):
    fdbLogTrace(4, tag, *argv)

def FDB_LOG_F(tag, *argv):
    fdbLogTrace(5, tag, argv)

# private function
def fdbusCtypes2buffer(cptr, length):
    """Convert ctypes pointer to buffer type.

    Parameters
    ----------
    cptr : ctypes.POINTER(ctypes.c_byte)
        Pointer to the raw memory region.
    length : int
        The length of the buffer.

    Returns
    -------
    buffer : bytearray
        The raw byte memory buffer.
    """
    #if not isinstance(cptr, ctypes.POINTER(ctypes.c_char)):
    #    raise TypeError('expected char pointer')
    if not bool(cptr) or not length:
        return None

    res = bytearray(length)
    rptr = (ctypes.c_byte * length).from_buffer(res)
    if not ctypes.memmove(rptr, cptr, length):
        return None
    return bytes(res)

class SubscribeItem(ctypes.Structure):
            _fields_ = [('event_code', ctypes.c_int), ('topic', ctypes.c_char_p)]

class ReturnMessage(ctypes.Structure):
    _fields_ = [('sid', ctypes.c_int),
                ('msg_code', ctypes.c_int),
                ('msg_data', ctypes.POINTER(ctypes.c_byte)), 
                ('data_size', ctypes.c_int),
                ('status', ctypes.c_int),
                ('msg_buffer', ctypes.c_void_p),
                ('topic', ctypes.c_char_p)]

fdb_client_online_fn_t = ctypes.CFUNCTYPE(None,                                  #return
                                          ctypes.c_void_p,                       #handle
                                          ctypes.c_int                           #sid
                                          )
fdb_client_offline_fn_t = ctypes.CFUNCTYPE(None,                                 #return
                                           ctypes.c_void_p,                      #handle
                                           ctypes.c_int                          #sid
                                           )
fdb_client_reply_fn_t = ctypes.CFUNCTYPE(None,                                   #return
                                         ctypes.c_void_p,                        #handle
                                         ctypes.c_int,                           #sid
                                         ctypes.c_int,                           #msg_code
                                         ctypes.POINTER(ctypes.c_byte),          #msg_data
                                         ctypes.c_int,                           #data_size
                                         ctypes.c_int,                           #status
                                         ctypes.c_void_p                         #user_data
                                         )
fdb_client_get_event_fn_t = ctypes.CFUNCTYPE(None,                               #return
                                             ctypes.c_void_p,                    #handle
                                             ctypes.c_int,                       #sid
                                             ctypes.c_int,                       #msg_code
                                             ctypes.c_char_p,                    #topic
                                             ctypes.POINTER(ctypes.c_byte),      #msg_data
                                             ctypes.c_int,                       #data_size
                                             ctypes.c_int,                       #status
                                             ctypes.c_void_p                     #user_data
                                         )
fdb_client_broadcast_fn_t = ctypes.CFUNCTYPE(None,                               #return
                                             ctypes.c_void_p,                    #handle
                                             ctypes.c_int,                       #sid
                                             ctypes.c_int,                       #event_code
                                             ctypes.POINTER(ctypes.c_byte),      #event_data
                                             ctypes.c_int,                       #data_size
                                             ctypes.c_char_p                     #topic
                                             )

# public function
# initialize FDBus; should be called before any call to FDBus
def fdbusStart(clib_path = None):
    global fdb_clib
    os_is = sys.platform.startswith
    if os_is("win32"):
        dll = "fdbus-clib.dll"
    else:
        dll = "libfdbus-clib.so"

    if clib_path:
        dll = os.path.join(clib_path, dll)
    fdb_clib = ctypes.CDLL(dll)
    fdb_clib.fdb_start()

# base class of FDBus client
class FdbusClient(object):
    # create FDBus client.
    # @name - name of client endpoint for debug purpose
    def __init__(self, name):
        global fdb_clib
        if fdb_clib is None:
            e = ValueError()
            e.strerror = 'fdbus is not started! Did fdbusStart() called?'
            raise(e)

        self.name = name
        fn_create = fdb_clib.fdb_client_create
        fn_create.restype = ctypes.c_void_p
        self.native = fn_create(name)
        fdb_clib.fdb_client_register_event_handle.argtypes = [ctypes.c_void_p,
                                                              fdb_client_online_fn_t,
                                                              fdb_client_offline_fn_t,
                                                              fdb_client_reply_fn_t,
                                                              fdb_client_get_event_fn_t,
                                                              fdb_client_broadcast_fn_t]
        self.online_func = self.getOnOnlineFunc()
        self.offline_func = self.getOnOfflineFunc()
        self.reply_func = self.getOnReplyFunc()
        self.get_event_func = self.getOnGetEventFunc()
        self.broadcast_func = self.getOnBroadcast()
        fdb_clib.fdb_client_register_event_handle(self.native,
                                                  self.online_func,
                                                  self.offline_func,
                                                  self.reply_func,
                                                  self.get_event_func,
                                                  self.broadcast_func)
    # private method
    def getOnOnlineFunc(self):
        def callOnOnline(handle, sid):
            self.onOnline(sid)
        return fdb_client_online_fn_t(callOnOnline)

    # private method
    def getOnOfflineFunc(self):
        def callOnOffline(handle, sid):
            self.onOffline(sid)
        return fdb_client_offline_fn_t(callOnOffline)

    # private method
    def getOnReplyFunc(self):
        def callOnReply(handle,
                        sid,
                        msg_code,
                        msg_data,
                        data_size,
                        status,
                        user_data):
            self.onReply(sid,
                         msg_code,
                         fdbusCtypes2buffer(msg_data, data_size),
                         status,
                         user_data)
        return fdb_client_reply_fn_t(callOnReply)

    # private method
    def getOnGetEventFunc(self):
        def callOnGetEvent(handle,
                           sid,
                           event,
                           topic,
                           event_data,
                           data_size,
                           status,
                           user_data):
            self.onGetEvent(sid,
                            event,
                            topic,
                            fdbusCtypes2buffer(event_data, data_size),
                            status,
                            user_data)
        return fdb_client_get_event_fn_t(callOnGetEvent)

    # private method
    def getOnBroadcast(self):
        def callOnBroadcast(handle,
                            sid,
                            event_code,
                            event_data,
                            data_size,
                            topic):
            self.onBroadcast(sid,
                             event_code,
                             fdbusCtypes2buffer(event_data, data_size),
                             topic)
        return fdb_client_broadcast_fn_t(callOnBroadcast)

    # public method
    # connect to server
    # @url - address of the server in format 'svc://server_name
    def connect(self, url):
        global fdb_clib
        fdb_clib.fdb_client_connect.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        fdb_clib.fdb_client_connect(self.native, castToChar(url))
        
    # public method
    # disconnect with server
    def disconnect(self):
        global fdb_clib
        fdb_clib.fdb_client_disconnect.argtypes = [ctypes.c_void_p]
        fdb_clib.fdb_client_disconnect(self.native)

    """
    public method
    invoke server method asynchronously: will return immediately and
        the reply will be received from onReply()
    @msg_code(int) - message code
    @msg_data(str) - data of the message
    @timeout(int) - timer of the call; if not reply before timeout,
        status will received
    @user_data(int) - user data provided by user and will be
        returned at onReply()
    @log_data(str) - extra information that will be printed in logsvc
    """
    def invoke_async(self,
                     msg_code,
                     msg_data = None,
                     timeout = 0,
                     user_data = None,
                     log_data = None):
        if msg_data is None:
            data_size = 0
        else:
            data_size = len(msg_data)
        global fdb_clib
        fdb_clib.fdb_client_invoke_async.argtypes = [ctypes.c_void_p,
                                                     ctypes.c_int,
                                                     ctypes.c_char_p,
                                                     ctypes.c_int,
                                                     ctypes.c_int,
                                                     ctypes.c_void_p,
                                                     ctypes.c_char_p]
        fdb_clib.fdb_client_invoke_async(self.native,
                                         msg_code,
                                         msg_data,
                                         data_size,
                                         timeout,
                                         user_data,
                                         castToChar(log_data))

    """
    public method
    invoke server method synchronously: will block until reply is received.
    @msg_code(int) - message code
    @msg_data(str) - data of the message
    @timeout(int) - timer of the call; if not reply before timeout,
        status will received
    @log_data(str) - extra information that will be printed in logsvc
    @return - data returned in dictionary:
        'sid'(int) - session id
        'msg_code'(int) - message code
        'msg_data'(str) - message data
        'status'(int) - return status
    """
    def invoke_sync(self, msg_code, msg_data = None, timeout = 0, log_data = None):
        global fdb_clib
        ret = ReturnMessage()
        if msg_data is None:
            data_size = 0
        else:
            data_size = len(msg_data)
        fdb_clib.fdb_client_invoke_sync.argtypes = [ctypes.c_void_p,
                                                    ctypes.c_int,
                                                    ctypes.c_char_p,
                                                    ctypes.c_int,
                                                    ctypes.c_int,
                                                    ctypes.c_char_p,
                                                    ctypes.POINTER(ReturnMessage)]
        fdb_clib.fdb_client_invoke_sync(self.native,
                                        msg_code,
                                        msg_data,
                                        data_size,
                                        timeout,
                                        castToChar(log_data),
                                        ctypes.byref(ret))
        return {'sid' : ret.sid,
                'msg_code' : ret.msg_code,
                'msg_data' : fdbusCtypes2buffer(ret.msg_data, ret.data_size),
                'status' : ret.status,
                'msg_buffer' : ret.msg_buffer}

    """
    public method
    invoke server method asynchronously; no reply is expected.
    @msg_code(int) - message code
    @msg_data(str) - data of the message
    @log_data(str) - extra information that will be printed in logsvc
    """
    def send(self, msg_code, msg_data = None, log_data = None):
        global fdb_clib
        if msg_data is None:
            data_size = 0
        else:
            data_size = len(msg_data)
        fdb_clib.fdb_client_send.argtypes = [ctypes.c_void_p,
                                             ctypes.c_int,
                                             ctypes.c_char_p,
                                             ctypes.c_int,
                                             ctypes.c_char_p]
        fdb_clib.fdb_client_send(self.native,
                                 msg_code,
                                 msg_data,
                                 data_size,
                                 castToChar(log_data))

    def publish(self, event, topic = None, event_data = None, log_data = None, always_update = False):
        global fdb_clib
        if event_data is None:
            data_size = 0
        else:
            data_size = len(event_data)

        fdb_clib.fdb_client_publish.argtypes = [ctypes.c_void_p,
                                                ctypes.c_int,
                                                ctypes.c_char_p,
                                                ctypes.c_char_p,
                                                ctypes.c_int,
                                                ctypes.c_char_p,
                                                ctypes.c_bool]
        fdb_clib.fdb_client_publish(self.native,
                                    event,
                                    castToChar(topic),
                                    event_data,
                                    data_size,
                                    castToChar(log_data),
                                    always_update)

    def get_async(self, event, topic = None, timeout = 0, user_data = None):
        global fdb_clib
        fdb_clib.fdb_client_get_event_async.argtypes = [ctypes.c_void_p,
                                                        ctypes.c_int,
                                                        ctypes.c_char_p,
                                                        ctypes.c_int,
                                                        ctypes.c_void_p
                                                        ]
        fdb_clib.fdb_client_get_event_async(self.native,
                                            event,
                                            castToChar(topic),
                                            timeout,
                                            user_data)

    def get_sync(self, event, topic = None, timeout = 0):
        global fdb_clib
        ret = ReturnMessage()
        fdb_clib.fdb_client_get_event_sync.argtypes = [ctypes.c_void_p,
                                                       ctypes.c_int,
                                                       ctypes.c_char_p,
                                                       ctypes.c_int,
                                                       ctypes.POINTER(ReturnMessage)
                                                       ]
        fdb_clib.fdb_client_get_event_sync(self.native,
                                           event,
                                           castToChar(topic),
                                           timeout,
                                           ctypes.byref(ret))
        return {'sid' : ret.sid,
                'event' : ret.msg_code,
                'topic' : topic,
                'event_data' : fdbusCtypes2buffer(ret.msg_data, ret.data_size),
                'status' : ret.status,
                'msg_buffer' : ret.msg_buffer}

    """
    public method
    subscribe list of events upon the server
    event_list(array of dict): list of events to subscribe.
        event_list[n]['event_code'](int) - event code or
        event_list[n]['group'](int) - event group
        event_list[n]['topic'](str) - topic
    """
    def subscribe(self, event_list):
        global fdb_clib
        class SubscribeItem(ctypes.Structure):
            _fields_ = [('event_code', ctypes.c_int), ('topic', ctypes.c_char_p)]

        subscribe_items = (SubscribeItem * len(event_list))()
        for i in range(len(event_list)):
            code = event_list[i].get('event_code', None)
            if not code:
                code = event_list[i].get('group', None)
                if code:
                    code = ((code & 0xff) << 24) | 0xffffff
            if code:
                subscribe_items[i].event_code = ctypes.c_int(code)
                subscribe_items[i].topic = ctypes.c_char_p(castToChar(event_list[i]['topic']))

        # what if topic is None?
        fdb_clib.fdb_client_subscribe.argtypes = [ctypes.c_void_p,
                                                  ctypes.POINTER(SubscribeItem),
                                                  ctypes.c_int]
        fdb_clib.fdb_client_subscribe(self.native, subscribe_items, len(subscribe_items))
        
    """
    public method
    unsubscribe list of events upon the server
    event_list - the same as subscribe()
    """
    def unsubscribe(self, event_list):
        global fdb_clib
        subscribe_items = (SubscribeItem * len(event_list))()
        for i in range(len(event_list)):
            subscribe_items[i].event_code = ctypes.c_int(event_list[i]['event_code'])
            subscribe_items[i].topic = ctypes.c_char_p(castToChar(event_list[i]['topic']))

        fdb_clib.fdb_client_unsubscribe.argtypes = [ctypes.c_void_p,
                                                    ctypes.POINTER(SubscribeItem),
                                                    ctypes.c_int]
        fdb_clib.fdb_client_unsubscribe(self.native, subscribe_items, len(subscribe_items))
    
    """
    Callback method and should be overrided
    will be called when the client is connected with server
    @sid - session id
    """
    def onOnline(self, sid):
        print('onOnline for client ', self.name)
    
    """
    Callback method
    will be called when the client is disconnected with server
    @sid(int) - session id
    """
    def onOffline(self, sid):
        print('onOffline for client ', self.name)
    
    """
    Callback method and should be overrided
    will be called when (asynchronous) method call is replied
    @sid(int) - session id
    @msg_code(int) - message code
    @msg_data(str) - message data
    @status(int) - status replied from server
    @user_data(int) - data given by invoke_async() and return to the user here
    """
    def onReply(self, sid, msg_code, msg_data, status, user_data):
        if msg_data is None:
            data_size = 0
        else:
            data_size = len(msg_data)
        print('onReply for client ', self.name,
              ', code: ', msg_code,
              ', size: ', data_size)

    def onGetEvent(self, sid, event, topic, event_data, status, user_data):
        if event_data is None:
            data_size = 0
        else:
            data_size = len(event_data)
        print('onGetEvent for client ', self.name,
              ', code: ', event,
              ', topic: ', topic,
              ', size: ', data_size)
        
    """
    Callback method and should be overrided
    will be called when events are broadcasted from server
    @sid(int) - session id
    @event_code(int) - event code
    @event_data(str) - event data
    @topic(str) - topic of the event
    """
    def onBroadcast(self, sid, event_code, event_data, topic):
        if event_data is None:
            data_size = 0
        else:
            data_size = len(event_data)
        print('onBroadcast for client ', self.name,
              ', code: ', event_code,
              ', topic: ', topic,
              ', size: ', data_size)

def releaseReturnMsg(ret_msg):
    global fdb_clib
    c_ret_msg = ReturnMessage()
    c_ret_msg.msg_buffer = ret_msg['msg_buffer']
    fdb_clib.fdb_client_release_return_msg.argtypes = [ctypes.POINTER(ReturnMessage)]
    fdb_clib.fdb_client_release_return_msg(ctypes.byref(c_ret_msg))


# class acting as handle to reply message to the calling client
class FdbusReplyHandle():
    def __init__(self, reply_handle):
        self.reply_handle = reply_handle
    
    """
    public method
    reply message to the calling client
    @msg_data(str) - message data
    @log_data(str) - extra information that will be printed in logsvc
    note that message code is the same as that of method call
    """
    def reply(self, msg_data = None, log_data = None):
        if msg_data is None:
            data_size = 0
        else:
            data_size = len(msg_data)
        global fdb_clib
        fdb_clib.fdb_message_reply.argtypes = [ctypes.c_void_p,
                                               ctypes.c_char_p,
                                               ctypes.c_int,
                                               ctypes.c_char_p]
        fdb_clib.fdb_message_reply(self.reply_handle,
                                   msg_data,
                                   data_size,
                                   castToChar(log_data))
    
    """
    public method
    broadcast message to the client subscribing the event
    @event_code(int) - event code
    @event_data(str) - event data
    @topic(str) - topic of the event
    @log_data(str) - extra information that will be printed in logsvc
    """
    def broadcast(self, event_code, event_data = None, topic = None, log_data = None):
        if event_data is None:
            data_size = 0
        else:
            data_size = len(event_data)
        global fdb_clib
        fdb_clib.fdb_message_broadcast.argtypes = [ctypes.c_void_p,
                                                   ctypes.c_int,
                                                   ctypes.c_char_p,
                                                   ctypes.c_char_p,
                                                   ctypes.c_int,
                                                   ctypes.c_void_p]
        fdb_clib.fdb_message_broadcast(self.reply_handle,
                                       event_code,
                                       castToChar(topic),
                                       event_data,
                                       data_size,
                                       log_data)
        
    """
    public method
    release resources occupied by the handle. should be called after method
    call is replied (with reply() or event is broadcasted (with broadcast()
    or memory leakage happens
    """
    def destroy(self):
        global fdb_clib
        fdb_clib.fdb_message_destroy.argtypes = [ctypes.c_void_p]
        fdb_clib.fdb_message_destroy(self.reply_handle)
        self.reply_handle = None
    
fdb_server_online_fn_t = ctypes.CFUNCTYPE(None,                             #return
                                          ctypes.c_void_p,                  #handle
                                          ctypes.c_int,                     #sid
                                          ctypes.c_byte                     #is_first
                                          )
fdb_server_offline_fn_t = ctypes.CFUNCTYPE(None,                            #return
                                           ctypes.c_void_p,                 #handle
                                           ctypes.c_int,                    #sid
                                           ctypes.c_byte                    #is_first
                                           )
fdb_server_invoke_fn_t = ctypes.CFUNCTYPE(None,                             #return
                                          ctypes.c_void_p,                  #handle
                                          ctypes.c_int,                     #sid
                                          ctypes.c_int,                     #msg_code
                                          ctypes.POINTER(ctypes.c_byte),    #msg_data
                                          ctypes.c_int,                     #data_size
                                          ctypes.c_void_p                   #reply_handle
                                          )
fdb_server_subscribe_fn_t = ctypes.CFUNCTYPE(None,                          #return
                                             ctypes.c_void_p,               #handle
                                             ctypes.POINTER(SubscribeItem), #subscribe_items
                                             ctypes.c_int,                  #nr_items
                                             ctypes.c_void_p                #reply_handle
                                             )
#base class of FDBus Server
class FdbusServer(object):
    # create FDBus server
    # name: name of the server for debug purpose
    def __init__(self, name):
        global fdb_clib
        if fdb_clib is None:
            e = ValueError()
            e.strerror = 'fdbus is not started! Did fdbusStart() called?'
            raise(e)
        self.name = name
        fn_create = fdb_clib.fdb_server_create
        fn_create.restype = ctypes.c_void_p
        self.native = fn_create(name)
        fdb_clib.fdb_server_register_event_handle.argtypes = [ctypes.c_void_p,
                                                              fdb_server_online_fn_t,
                                                              fdb_server_offline_fn_t,
                                                              fdb_server_invoke_fn_t,
                                                              fdb_server_subscribe_fn_t]
        self.online_func = self.getOnOnlineFunc()
        self.offline_func = self.getOnOfflineFunc()
        self.invoke_func = self.getOnInvokeFunc()
        self.subscribe_func = self.getOnSubscribeFunc()
        fdb_clib.fdb_server_register_event_handle(self.native,
                                                  self.online_func,
                                                  self.offline_func,
                                                  self.invoke_func,
                                                  self.subscribe_func)

    # private method
    def getOnOnlineFunc(self):
        def callOnOnline(handle, sid, is_first):
            self.onOnline(sid, bool(is_first))
        return fdb_server_online_fn_t(callOnOnline)

    # private method
    def getOnOfflineFunc(self):
        def callOnOffline(handle, sid, is_last):
            self.onOffline(sid, bool(is_last))
        return fdb_server_offline_fn_t(callOnOffline)

    # private method
    def getOnInvokeFunc(self):
        def callOnInvoke(handle, sid, msg_code, msg_data, data_size, reply_handle):
            self.onInvoke(sid,
                          msg_code,
                          fdbusCtypes2buffer(msg_data, data_size),
                          FdbusReplyHandle(reply_handle))
        return fdb_server_invoke_fn_t(callOnInvoke)

    # private method
    def getOnSubscribeFunc(self):
        def callOnSubscribe(handle, subscribe_items, nr_items, reply_handle):
            p = ctypes.cast(subscribe_items, ctypes.POINTER(SubscribeItem * nr_items))
            c_items = p.contents
            items = []
            for i in range(nr_items):
                items.append({'event_code': c_items[i].event_code,
                              'topic' : str(c_items[i].topic)})

            self.onSubscribe(items,
                             FdbusReplyHandle(reply_handle))
        return fdb_server_subscribe_fn_t(callOnSubscribe)

    # public method
    # bind server with a address
    # @url(str) - address of the server in format 'svc://server_name
    def bind(self, url):
        global fdb_clib
        fdb_clib.fdb_server_bind.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        fdb_clib.fdb_server_bind(self.native, castToChar(url))
        
    # public method
    # unbind server with a address
    def unbind(self):
        global fdb_clib
        fdb_clib.fdb_server_unbind.argtypes = [ctypes.c_void_p]
        fdb_clib.fdb_server_unbind(self.native)
    
    """
    public method
    broadcast event to the clients subscribed the event
    @event_code(int) - code of the event
    @event_data(str) - data of the event
    @topic(str) - topic of the event
    @log_data(str) - extra information that will be printed in logsvc
    """
    def broadcast(self, event_code, event_data = None, topic = None, log_data = None):
        global fdb_clib
        if event_data is None:
            data_size = 0
        else:
            data_size = len(event_data)
        fdb_clib.fdb_server_broadcast.argtypes = [ctypes.c_void_p,
                                                   ctypes.c_int,
                                                   ctypes.c_char_p,
                                                   ctypes.c_char_p,
                                                   ctypes.c_int,
                                                   ctypes.c_void_p]
        fdb_clib.fdb_server_broadcast(self.native,
                                       event_code,
                                       castToChar(topic),
                                       event_data,
                                       data_size,
                                       log_data)

    def enable_event_cache(self, enable):
        global fdb_clib
        fdb_clib.fdb_server_enable_event_cache.argtypes = [ctypes.c_void_p,
                                                           ctypes.c_bool]
        fdb_clib.fdb_server_enable_event_cache(self.native, enable)

    def init_event_cache(self, event, topic, event_data, always_update):
        global fdb_clib
        if event_data is None:
            data_size = 0
        else:
            data_size = len(event_data)

        fdb_clib.fdb_server_init_event_cache.argtypes = [ctypes.c_void_p,
                                                         ctypes.c_int,
                                                         ctypes.c_char_p,
                                                         ctypes.c_char_p,
                                                         ctypes.c_int,
                                                         ctypes.c_bool]
        fdb_clib.fdb_server_init_event_cache(self.native,
                                             event,
                                             castToChar(topic),
                                             event_data,
                                             data_size,
                                             always_update)

    """
    Callback method and should be overrided
    called when a connect is connected
    @sid(int) - session ID
    @is_first(bool) - True if this is the first connected client; otherwise False
    """
    def onOnline(self, sid, is_first):
        print('onOnline for server ', self.name, ', first: ', is_first)
    
    """
    Callback method and should be overrided
    called when a client is connected
    @sid(int) - session ID
    @is_first(bool) - True if this is the last connected client; otherwise False
    """
    def onOffline(self, sid, is_last):
        print('onOffline for server ', self.name, ', last: ', is_last)

    """
    Callback method and should be overrided
    called when methods are called from clients
    @sid(int) - session ID
    @msg_code(int) - code of message
    @msg_data(str) - data of message
    @reply_handle(FdbusReplyHandle) - handle used to reply message to the
        calling client
    """
    def onInvoke(self, sid, msg_code, msg_data, reply_handle):
        if msg_data is None:
            data_size = 0
        else:
            data_size = len(msg_data)
        print('onInvoke for server ', self.name,
              ', code: ', msg_code,
              ', size: ', data_size)

    """
    Callback method and should be overrided
    called when clients are subscribing events
    @event_list(int) - list of events subscribed. refer to FdbusClient::subscribe()
    @reply_handle(FdbusReplyHandle) - handle used to reply initial value of events
        to calling client
    """
    def onSubscribe(self, event_list, reply_handle):
        print('onSubscribe for server ', self.name)
        for i in range(len(event_list)):
            print('    event: ', event_list[i]['event_code'],
                  ', topic: ', event_list[i]['topic'])

