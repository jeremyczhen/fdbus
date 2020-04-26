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
        print('MyTestServer - onInvoke: code: ', msg_code, ', data: ', msg_data)
        reply_handle.reply('a brown fox jump over sleepy dog')
        reply_handle.destroy()

    def onSubscribe(self, subscribe_items, reply_handle):
        super(MyTestServer, self).onSubscribe(subscribe_items, reply_handle)
        for i in range(len(subscribe_items)):
            reply_handle.broadcast(subscribe_items[i]['event_code'],
                                   '\x33\x34\x35\x36\x37\x38\x39\x40',
                                   subscribe_items[i]['topic'])
        reply_handle.destroy()

fdbus.fdbusStart()
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
        server_list[i].broadcast(101, 'hello, world', 'topic-2')
        time.sleep(1)
