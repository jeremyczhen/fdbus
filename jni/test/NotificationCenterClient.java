/* Copyright (C) 2015   Jeremy Chen jeremy_cz@yahoo.com
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

import ipc.fdbus.FdbusClient;
import ipc.fdbus.FdbusClientListener;
import ipc.fdbus.SubscribeItem;
import ipc.fdbus.Fdbus;
import ipc.fdbus.FdbusMessage;
import ipc.fdbus.NFdbExample;
import ipc.fdbus.Example.FdbusProtoBuilder;
import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;
import ipc.fdbus.Example.CPerson;
import ipc.fdbus.FdbusDeserializer;
import ipc.fdbus.FdbusParcelable;
import ipc.fdbus.FdbusParcelable.TextFormatter;

public class NotificationCenterClient
{
    private class FdbusNotificationClient extends TimerTask implements FdbusClientListener
    {
        public final static int CONFIG_SYNC_INVOKE = 0;

        private int mSongId;
        private Timer mTimer;
        private boolean mTimerRunning;
        
        private FdbusClient mClient;
        private boolean mIsPublisher;

        public FdbusNotificationClient(String name, boolean is_publisher)
        {
            mClient = new FdbusClient(name);
            mSongId = 0;
            mTimer = null;
            mTimerRunning = false;
            mIsPublisher = is_publisher;
        }

        public void startServerInvoker()
        {
            mTimer = new Timer();
            mTimer.schedule(this, 300, 300);
        }

        public void stopServerInvoker()
        {
            if (mTimer != null)
            {
                mTimer.cancel();
                while (mTimerRunning)
                {
                    try{Thread.sleep(2);}catch(InterruptedException e){System.out.println(e);}
                }
            }
        }

        public FdbusClient client()
        {
            return mClient;
        }
        
        public void onOnline(int sid)
        {
            System.out.println(mClient.endpointName() + ": onOnline is received: " + sid);
            if (!mIsPublisher)
            {
                ArrayList<SubscribeItem> subscribe_items = new ArrayList<SubscribeItem>();
                subscribe_items.add(SubscribeItem.newEvent(NFdbExample.FdbMediaSvcMsgId.REQ_METADATA_VALUE, "java topic1"));
                mClient.subscribe(subscribe_items);
            }
        }
        public void onOffline(int sid)
        {
            System.out.println(mClient.endpointName() + ": onOffline is received: " + sid);
        }

        private void handleEvent(FdbusMessage msg, String type)
        {
            if (msg.returnValue() != Fdbus.FDB_ST_OK)
            {
                System.out.println(mClient.endpointName() + 
                        ": Error is received at " + type + ", error code: " + msg.returnValue());
                return;
            }

            switch (msg.code())
            {
                case NFdbExample.FdbMediaSvcMsgId.REQ_METADATA_VALUE:
                {
                    try {
                        NFdbExample.SongId song_id = NFdbExample.SongId.parseFrom(msg.byteArray());
                        System.out.println(type + ": code: " + msg.code() + ", topic: " + msg.topic() + ", song id: " + song_id.getId());
                    } catch (Exception e) {
                        System.out.println(e);
                    }
                }
                break;
                default:
                break;
            }
        }
        
        public void onReply(FdbusMessage msg)
        {
        }

        public void onGetEvent(FdbusMessage msg)
        {
            handleEvent(msg, "onGetEvent(async)");
        }
        
        public void onBroadcast(FdbusMessage msg)
        {
            handleEvent(msg, "onBroadcast");
        }

        public void run()
        {
            mTimerRunning = true;
            if (mIsPublisher)
            {
                NFdbExample.SongId.Builder proto_builder = NFdbExample.SongId.newBuilder();
                proto_builder.setId(mSongId);
                NFdbExample.SongId song_id = proto_builder.build();
                FdbusProtoBuilder builder = new FdbusProtoBuilder(song_id);
                mClient.publish(NFdbExample.FdbMediaSvcMsgId.REQ_METADATA_VALUE, "java topic1", builder, false);
            }
            else
            {
                mClient.getAsync(NFdbExample.FdbMediaSvcMsgId.REQ_METADATA_VALUE, "java topic1", null, 0);
                FdbusMessage msg = mClient.getSync(NFdbExample.FdbMediaSvcMsgId.REQ_METADATA_VALUE, "java topic1", 0);
                handleEvent(msg, "onGetEvent(sync)");
            }
            mTimerRunning = false;
        }
    }

    private FdbusNotificationClient createClient(String name, boolean is_publisher)
    {
        FdbusNotificationClient clt = new FdbusNotificationClient(name, is_publisher);
        clt.client().setListener(clt);
        return clt;
    }
    
    public static void main(String[] args)
    {
        Fdbus fdbus = new Fdbus();
        NotificationCenterClient clt = new NotificationCenterClient();

        if (args.length != 2)
        {
            System.out.println("0/1 server_name");
            return;
        }
        boolean is_publish = false;
        int type = Integer.parseInt(args[0]);
        if (type == 1)
        {
            is_publish = true;
        }
         System.out.println("args: " + args[0] + "|" + args[1]);
         System.out.println("is publish: " + is_publish);

        String server_name = args[1];
        FdbusNotificationClient client = clt.createClient(server_name + "_client", is_publish);
        client.client().connect("svc://" + server_name);
        client.startServerInvoker();
        
        try{Thread.sleep(500000);}catch(InterruptedException e){System.out.println(e);}
    }
}

