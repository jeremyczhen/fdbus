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

class MyTestServer(fdbus.FdbusServer):
    def __init__(self, name):
        super(MyTestServer, self).__init__(name)
        
    def onOnline(self, sid, is_first):
        print('MyTestServer: onOnline')
        super(MyTestServer, self).onOnline(sid, is_first)

    def onOffline(self, sid, is_last):
        print('MyTestServer: onOffline')
        super(MyTestServer, self).onOffline(sid, is_last)

    def onInvoke(self, sid, msg_code, msg_data, reply_handle):
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

    def onSubscribe(self, subscribe_items, reply_handle):
        super(MyTestServer, self).onSubscribe(subscribe_items, reply_handle)
        for i in range(len(subscribe_items)):
            reply_handle.broadcast(subscribe_items[i]['event_code'],
                                   get_elapse_time(),
                                   subscribe_items[i]['topic'])
        reply_handle.destroy()

fdbus.fdbusStart(os.getenv('FDB_CLIB_PATH'))
server_list = []
nr_servers = len(sys.argv) - 1
for i in range(nr_servers):
    name = sys.argv[i+1]
    url = 'svc://' + name
    server = MyTestServer(name)
    server_list.append(server)
    server.bind(url)
    
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
