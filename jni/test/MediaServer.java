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
import ipc.fdbus.Example.MyFdbusMessageEncoder;
import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;
import ipc.fdbus.Example.CPerson;
import ipc.fdbus.FdbusSerializer;

public class MediaServer
{
    private class FdbusMediaServer extends TimerTask implements FdbusServerListener
    {
        int mElapseTime;
    
        private FdbusServer mServer;
        
        public FdbusMediaServer(String name)
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
                case NFdbExample.FdbMediaSvcMsgId.REQ_METADATA_VALUE:
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
                    msg.reply(now_playing);
                }
                break;
                case NFdbExample.FdbMediaSvcMsgId.REQ_RAWDATA_VALUE:
                {
                    CPerson[] persons = new CPerson[2];
                    persons[0] = new CPerson();
                    persons[0].mAddress = "Shanghai";
                    persons[0].mAge = 22;
                    persons[0].mName = "Zhang San";
                    persons[0].mCars = new CPerson.CCar[2];
                    persons[0].mCars[0] = new CPerson.CCar();
                    persons[0].mCars[0].mBrand = "Hongqi";
                    persons[0].mCars[0].mModel = "H5";
                    persons[0].mCars[0].mPrice = 400000;
                    persons[0].mCars[1] = new CPerson.CCar();
                    persons[0].mCars[1].mBrand = "ROEWE";
                    persons[0].mCars[1].mModel = "X5";
                    persons[0].mCars[1].mPrice = 200000;

                    persons[1] = new CPerson();
                    persons[1].mAddress = "Guangzhou";
                    persons[1].mAge = 22;
                    persons[1].mName = "Li Si";
                    persons[1].mCars = new CPerson.CCar[2];
                    persons[1].mCars[0] = new CPerson.CCar();
                    persons[1].mCars[0].mBrand = "Chuanqi";
                    persons[1].mCars[0].mModel = "H5";
                    persons[1].mCars[0].mPrice = 400000;
                    persons[1].mCars[1] = new CPerson.CCar();
                    persons[1].mCars[1].mBrand = "Toyota";
                    persons[1].mCars[1].mModel = "X5";
                    persons[1].mCars[1].mPrice = 200000;

                    FdbusSerializer serializer = new FdbusSerializer();
                    serializer.in(persons);
                    msg.reply(serializer.getBuffer());
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
                    case NFdbExample.FdbMediaSvcMsgId.NTF_ELAPSE_TIME_VALUE:
                    {
                        if (item.topic().equals("my_filter"))
                        {
                            NFdbExample.ElapseTime.Builder builder = NFdbExample.ElapseTime.newBuilder();
                            builder.setHour(0);
                            builder.setMinute(0);
                            builder.setSecond(0);
                            NFdbExample.ElapseTime et = builder.build();
                            msg.broadcast(item.code(), item.topic(), et);
                        }
                        else if (item.topic().equals("raw_buffer"))
                        {
                            System.out.println(mServer.endpointName() + ": subscribe of raw data is received.");
                        }
                    }
                    break;
                    case NFdbExample.FdbMediaSvcMsgId.NTF_MANUAL_UPDATE_VALUE:
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
            mServer.broadcast(NFdbExample.FdbMediaSvcMsgId.NTF_ELAPSE_TIME_VALUE, "my_filter", et);
        }
    }

    Timer mTimer;
    public void startBroadcast(TimerTask tt)
    {
        mTimer = new Timer();
        mTimer.schedule(tt, 400, 400);
    }
    
    private FdbusMediaServer createServer(String name)
    {
        FdbusMediaServer svr = new FdbusMediaServer(name);
        svr.server().setListener(svr);
        return svr;
    }

    public static void main(String[] args)
    {
        Fdbus fdbus = new Fdbus(new MyFdbusMessageEncoder());
        MediaServer svr = new MediaServer();

        ArrayList<FdbusMediaServer> servers = new ArrayList<FdbusMediaServer>();
        for (String arg : args)
        {
            FdbusMediaServer server = svr.createServer(arg + "_server");
            server.server().bind("svc://" + arg);
            svr.startBroadcast(server);
            
            servers.add(server);
        }
        
        try{Thread.sleep(500000);}catch(InterruptedException e){System.out.println(e);}
        //Thread.sleep(5000000);
    }
}


