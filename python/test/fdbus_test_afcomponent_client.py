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

import fdbus
import sys
import time
import os
import Example_pb2 as ex

def process_response(code, resp_data, tag):
    if code == ex.REQ_METADATA:
        resp_info = ex.NowPlayingDetails()
        resp_info.ParseFromString(resp_data)
        print(tag, resp_info)
    else:
        print('unknown message code is received: ', code)

class MyReplyClosure(fdbus.ReplyClosure):
    def handleReply(self, sid, msg_code, msg_data, status, user_data):
        if status == 0:
            process_response(msg_code, msg_data, 'handleMessage: ')

class MyConnectionHandle(fdbus.ConnectionClosure):
    def handleConnection(self, sid, is_online, first_or_last):
        print('Connection status is changed - sid: ', sid, ', online: ', is_online, ', f or l: ', first_or_last)

class MyEventHandle(fdbus.EventClosure):
    def handleEvent(self, sid, event_code, event_data, topic):
        if event_code == ex.NTF_ELAPSE_TIME:
            et = ex.ElapseTime()
            et.ParseFromString(event_data)
            print('onBroadcast: \n', et)
        elif event_code == ex.NTF_MEDIAPLAYER_CREATED:
            print('onBroadcast: event: ', event_code, ', topic: ', topic)
        else:
            print('onBroadcast - unknown event: ', event_code)

conn_callback = MyConnectionHandle()
evt_callback = MyEventHandle()
event_handle_tbl = [
     {'code' : ex.NTF_ELAPSE_TIME, 'topic' : None, 'callback' : evt_callback.getEventCallback()},
     {'code' : ex.NTF_MEDIAPLAYER_CREATED, 'topic' : 'topic-1', 'callback' : evt_callback.getEventCallback()},
     {'code' : ex.NTF_MEDIAPLAYER_CREATED, 'topic' : 'topic-2', 'callback' : evt_callback.getEventCallback()}]

fdbus.fdbusStart(os.getenv('FDB_CLIB_PATH'))
component = fdbus.FdbusAfComponent("default component")
client_list = []
nr_clients = len(sys.argv) - 1
for i in range(nr_clients):
    name = sys.argv[i+1]
    client = component.queryService(name, event_handle_tbl, conn_callback.getConnectionCallback())
    client_list.append(client)

cb = MyReplyClosure()
song_id = 0
while True:
    req = ex.SongId()
    for i in range(nr_clients):
        """
        #req.id = song_id
        #song_id += 1
        #client_list[i].invoke_async(ex.REQ_METADATA, req.SerializeToString())
        #time.sleep(1)
        """

        req.id = song_id
        song_id += 1
        msg = client_list[i].invoke_sync(ex.REQ_METADATA, req.SerializeToString())
        if msg['status']:
            print('Error for sync invoke: ', msg['status'])
        else:
            process_response(msg['msg_code'], msg['msg_data'], 'sync reply: ')
        fdbus.releaseReturnMsg(msg)
        fdbus.FDB_LOG_E('fdb_py_clt', 'name: ', 'sdcard', 'size', 1000)
        time.sleep(1)

        req.id = song_id
        song_id += 1
        client_list[i].invoke_callback(cb.getReplyCallback(), ex.REQ_METADATA, req.SerializeToString())
        time.sleep(1)

