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

is_publish = False
el_time = 0
def get_elapse_time():
    global el_time
    et = ex.ElapseTime()
    et.hour = 13
    et.minute = 59
    et.second = el_time
    el_time += 1
    return et.SerializeToString()

def process_response(event, topic, event_data, status, tag):
    if status:
        print(tag, ': error! status: ', status)
        return

    if event == ex.NTF_ELAPSE_TIME:
        et = ex.ElapseTime()
        et.ParseFromString(event_data)
        print(tag, ': event: ', ex.NTF_ELAPSE_TIME, ', data: ', et, ', topic: ', topic)
    else:
        print('unknown event is received: ', code)

class NotificationCenterClient(fdbus.FdbusClient):
    def __init__(self, name):
        super(NotificationCenterClient, self).__init__(name)
        
    def onOnline(self, sid):
        print('NotificationCenterClient: onOnline')
        if not is_publish:
            subscribe_list = [{'event_code' : ex.NTF_ELAPSE_TIME, 'topic' : 'python topic 1'}]
            self.subscribe(subscribe_list)

    def onOffline(self, sid):
        print('NotificationCenterClient: onOffline')

    def onReply(self, sid, msg_code, msg_data, status, user_data):
        print('NotificationCenterClient: onReply')

    def onGetEvent(self, sid, event, topic, event_data, status, user_data):
        process_response(event, topic, event_data, status, 'onGetEvent(async)')
 
    def onBroadcast(self, sid, event, event_data, topic):
        process_response(event, topic, event_data, 0, 'onBroadcast')

fdbus.fdbusStart(os.getenv('FDB_CLIB_PATH'))
if len(sys.argv) < 3:
    print('Usage: 0/1 server_name')
    exit(0)

if sys.argv[1] == '1':
    is_publish = True

server_name = sys.argv[2]
url = 'svc://' + server_name
client = NotificationCenterClient(server_name)
client.connect(url)

song_id = 0
while True:
    if is_publish:
        client.publish(ex.NTF_ELAPSE_TIME, 'python topic 1', get_elapse_time())
    else:
        client.get_async(ex.NTF_ELAPSE_TIME, 'python topic 1')
        msg = client.get_sync(ex.NTF_ELAPSE_TIME, 'python topic 1')
        process_response(msg['event'], msg['topic'], msg['event_data'], msg['status'], "onGetEvent(sync)")
    time.sleep(1)
