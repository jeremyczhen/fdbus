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

el_time = 0
def get_elapse_time():
    global el_time
    et = ex.ElapseTime()
    et.hour = 13
    et.minute = 59
    et.second = el_time
    el_time += 1
    return et.SerializeToString()

class MyConnectionHandle(fdbus.ConnectionClosure):
    def handleConnection(self, sid, is_online, is_first):
        print('Connection status is changed - sid: ', sid, ', online: ', is_online, ', first: ', is_first)

class MyMessageHandle(fdbus.MessageClosure):
    def handleMessage(self, sid, msg_code, msg_data, reply_handle):
        if msg_code == ex.REQ_METADATA:
            song_id = ex.SongId()
            song_id.ParseFromString(msg_data)
            print('onInvoke: song id is: ', song_id.id)
            now_playing = ex.NowPlayingDetails()
            now_playing.artist = 'The Weeknd'
            now_playing.album = 'Blinding Lights - Single'
            now_playing.genre = 'Alternative R&B'
            now_playing.title = 'Blinding Lights'
            now_playing.file_name = 'file-1'
            now_playing.folder_name = 'folder-1'
            now_playing.elapse_time = 100
            reply_handle.reply(now_playing.SerializeToString())
            reply_handle.destroy()
        else:
            print('Invoke is received - sid: ', sid, ', code: ', msg_code)

conn_callback = MyConnectionHandle()
msg_callback = MyMessageHandle()
message_handle_tbl = [
     {'msg_code' : ex.REQ_METADATA, 'callback' : msg_callback.getMessageCallback()},
     {'msg_code' : ex.REQ_CREATE_MEDIAPLAYER, 'callback' : msg_callback.getMessageCallback()}]

fdbus.fdbusStart(os.getenv('FDB_CLIB_PATH'))
component = fdbus.FdbusAfComponent("default component")
server_list = []
nr_servers = len(sys.argv) - 1
for i in range(nr_servers):
    name = sys.argv[i+1]
    server = component.offerService(name, message_handle_tbl, conn_callback.getConnectionCallback())
    server_list.append(server)

while True:
    for i in range(nr_servers):
        server_list[i].broadcast(ex.NTF_ELAPSE_TIME, get_elapse_time())
        server_list[i].broadcast(ex.NTF_MEDIAPLAYER_CREATED,
                                 fdbus.castToChar('hello, world!'),
                                 'topic-1')
        server_list[i].broadcast(ex.NTF_MEDIAPLAYER_CREATED,
                                 fdbus.castToChar('good morning!'),
                                 'topic-2')
        fdbus.FDB_LOG_E('fdb_py_svc', 'name: ', 'John', 'weigth', 120, 'pound')
        time.sleep(1)

