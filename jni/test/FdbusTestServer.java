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

import ipc.fdbus.FdbusServer;
import ipc.fdbus.FdbusServerListener;
import ipc.fdbus.SubscribeItem;
import ipc.fdbus.Fdbus;
import ipc.fdbus.FdbusMessage;
import ipc.fdbus.NFdbExample;
import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;

public class FdbusTestServer 
{
    private class myTestServer extends TimerTask implements FdbusServerListener
    {
        public final static int REQ_METADATA = 0;
        public final static int REQ_RAWDATA = 1;
        public final static int REQ_CREATE_MEDIAPLAYER = 2;
        public final static int NTF_ELAPSE_TIME = 3;
        public final static int NTF_MEDIAPLAYER_CREATED = 4;
        public final static int NTF_MANUAL_UPDATE = 5;

        
        int mElapseTime;
    
        private FdbusServer mServer;
        
        public myTestServer(String name)
        {
            mServer = new FdbusServer(name);
            mElapseTime = 0;
        }

        public FdbusServer server()
        {
            return mServer;
        }
        
        public void onOnline(int sid, boolean is_first)
        {
            System.out.println(mServer.endpointName() + ": onOnline is received: " + is_first);
        }
        
        public void onOffline(int sid, boolean is_last)
        {
            System.out.println(mServer.endpointName() + ": onOffline is received: " + is_last);
        }

        public void onInvoke(FdbusMessage msg)
        {
            switch (msg.code())
            {
                case REQ_METADATA:
                {
                    try {
                        NFdbExample.SongId song_id = NFdbExample.SongId.parseFrom(msg.byteArray());
                        System.out.println(mServer.endpointName() + 
                                    ": onInvoke: metadata request is received. song id: " + song_id.getId());
                    } catch (Exception e) {
                        System.out.println(e);
                    }
                    
                    NFdbExample.NowPlayingDetails.Builder builder = NFdbExample.NowPlayingDetails.newBuilder();
                    builder.setArtist("Artist from Java");
                    builder.setAlbum("Album from Java");
                    builder.setGenre("Genre from Java");
                    builder.setTitle("Title from Java");
                    builder.setFileName("Filename from Java");
                    builder.setElapseTime(mElapseTime++);
                    NFdbExample.NowPlayingDetails now_playing = builder.build();
                    msg.reply(now_playing.toByteArray());
                }
                break;
                default:
                break;
            }
        }
        
        public void onSubscribe(FdbusMessage msg, ArrayList<SubscribeItem> sub_list)
        {
            System.out.println(mServer.endpointName() + ": onsubscribe: size is: " + sub_list.size());
            for (int i = 0; i < sub_list.size(); ++i)
            {
                SubscribeItem item = sub_list.get(i);
                System.out.println(mServer.endpointName() + 
                            ": onSubscribe: code: " + item.code() + ", topic: " + item.topic());
                switch (item.code())
                {
                    case NTF_ELAPSE_TIME:
                    {
                        if (item.topic().equals("my_filter"))
                        {
                            NFdbExample.ElapseTime.Builder builder = NFdbExample.ElapseTime.newBuilder();
                            builder.setHour(0);
                            builder.setMinute(0);
                            builder.setSecond(0);
                            NFdbExample.ElapseTime et = builder.build();
                            msg.broadcast(item.code(), item.topic(), et.toByteArray());
                        }
                        else if (item.topic().equals("raw_buffer"))
                        {
                            System.out.println(mServer.endpointName() + ": subscribe of raw data is received.");
                        }
                    }
                    break;
                    case NTF_MANUAL_UPDATE:
                    break;
                    default:
                    break;
                }
            }
        }

        public void run()
        {
            NFdbExample.ElapseTime.Builder builder = NFdbExample.ElapseTime.newBuilder();
            builder.setHour(0);
            builder.setMinute(0);
            builder.setSecond(mElapseTime++);
            NFdbExample.ElapseTime et = builder.build();
            mServer.broadcast(NTF_ELAPSE_TIME, "my_filter", et.toByteArray());
        }
    }

    Timer mTimer;
    public void startBroadcast(TimerTask tt)
    {
        mTimer = new Timer();
        mTimer.schedule(tt, 400, 400);
    }
    
    private myTestServer createServer(String name)
    {
        myTestServer svr = new myTestServer(name);
        svr.server().setListener(svr);
        return svr;
    }

    public static void main(String[] args)
    {
        Fdbus fdbus = new Fdbus();
        FdbusTestServer svr = new FdbusTestServer();

        ArrayList<myTestServer> servers = new ArrayList<myTestServer>();
        for (String arg : args)
        {
            myTestServer server = svr.createServer(arg + "_server");
            server.server().bind("svc://" + arg);
            svr.startBroadcast(server);
            
            servers.add(server);
        }
        
        try{Thread.sleep(500000);}catch(InterruptedException e){System.out.println(e);}
        //Thread.sleep(5000000);
    }
}


