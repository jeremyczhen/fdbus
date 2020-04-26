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

class MyTestClient(fdbus.FdbusClient):
    def __init__(self, name):
        super(MyTestClient, self).__init__(name)
        
    def onOnline(self, sid):
        print('MyTestClient: onOnline')
        subscribe_list = [{'event_code' : 100, 'topic' : 'topic-1'},
                          {'event_code' : 101, 'topic' : 'topic-2'}]
        self.subscribe(subscribe_list)
        super(MyTestClient, self).onOnline(sid)

    def onOffline(self, sid):
        print('MyTestClient: onOffline')
        super(MyTestClient, self).onOffline(sid)

    def onReply(self, sid, msg_code, msg_data, status, user_data):
        print('MyTestClient: onReply: code: ', msg_code, ', data: ', msg_data)
        
    def onBroadcast(self, sid, msg_code, msg_data, topic):
        print('MyTestClient: onBroadcast: code: ', msg_code, ', data: ', msg_data, ', topic: ', topic)
        
fdbus.fdbusStart()
client_list = []
nr_clients = len(sys.argv) - 1
for i in range(nr_clients):
    name = sys.argv[i+1]
    url = 'svc://' + name
    client = MyTestClient(name)
    client_list.append(client)
    client.connect(url)
    
while True:
    for i in range(nr_clients):
        #client_list[i].invoke_async(200, 'hello, world')
        client_list[i].invoke_async(200)
        time.sleep(1)
        msg = client_list[i].invoke_sync(201, '\x21\x22\x23\x24\x25\x26')
        if msg['status']:
            print('Error for sync invoke: ', msg['status'])
        else:
            print('return from sync invoke - code: ', msg['msg_code'], ', value: ', msg['msg_data'])
        fdbus.releaseReturnMsg(msg)
        time.sleep(1)
