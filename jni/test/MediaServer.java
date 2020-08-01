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
import ipc.fdbus.FdbusSimpleBuilder;
import ipc.fdbus.Example.FdbusProtoBuilder;
import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;
import ipc.fdbus.Example.CPerson;

public class MediaServer
{
    private class FdbusMediaServer extends TimerTask implements FdbusServerListener
    {
        int mElapseTime;
    
        private FdbusServer mServer;
        private Timer mTimer;
        private boolean mTimerRunning;
        
        public FdbusMediaServer(String name)
        {
            mServer = new FdbusServer(name);
            mElapseTime = 0;
            mTimer = null;
            mTimerRunning = false;
        }

        public void startBroadcast()
        {
            mTimer = new Timer();
            mTimer.schedule(this, 400, 400);
        }

        public void stopBroadcast()
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
                    
                    NFdbExample.NowPlayingDetails.Builder proto_builder =
                                                NFdbExample.NowPlayingDetails.newBuilder();
                    proto_builder.setArtist("Artist from Java");
                    proto_builder.setAlbum("Album from Java");
                    proto_builder.setGenre("Genre from Java");
                    proto_builder.setTitle("Title from Java");
                    proto_builder.setFileName("Filename from Java");
                    proto_builder.setElapseTime(mElapseTime++);
                    NFdbExample.NowPlayingDetails now_playing = proto_builder.build();
                    FdbusProtoBuilder builder = new FdbusProtoBuilder(now_playing);
                    msg.reply(builder);
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
                    persons[0].mPrivateInfo = new byte[][]{
                                                    {12, 23, 34, 45, 56, 67, 78, 89, 90},
                                                    {9, 98, 87, 76, 65, 54, 43, 32, 21}
                                                };

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
                    persons[1].mPrivateInfo = new byte[][]{
                                                    {12, 23, 34, 45, 56, 67, 78, 89, 90},
                                                    {9, 98, 87, 76, 65, 54, 43, 32, 21},
                                                    {12, 23, 34, 45, 56, 67, 78, 89, 90},
                                                    {9, 98, 87, 76, 65, 54, 43, 32, 21}
                                                };

                    FdbusSimpleBuilder builder = new FdbusSimpleBuilder(persons);
                    msg.reply(builder);
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
                            NFdbExample.ElapseTime.Builder proto_builder =
                                                    NFdbExample.ElapseTime.newBuilder();
                            proto_builder.setHour(0);
                            proto_builder.setMinute(0);
                            proto_builder.setSecond(0);
                            NFdbExample.ElapseTime et = proto_builder.build();
                            FdbusProtoBuilder builder = new FdbusProtoBuilder(et);
                            msg.broadcast(item.code(), item.topic(), builder);
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
            mTimerRunning = true;
            NFdbExample.ElapseTime.Builder proto_builder = NFdbExample.ElapseTime.newBuilder();
            proto_builder.setHour(0);
            proto_builder.setMinute(0);
            proto_builder.setSecond(mElapseTime++);
            NFdbExample.ElapseTime et = proto_builder.build();
            FdbusProtoBuilder builder = new FdbusProtoBuilder(et);
            mServer.broadcast(NFdbExample.FdbMediaSvcMsgId.NTF_ELAPSE_TIME_VALUE, "my_filter", builder);
            mTimerRunning = false;
        }
    }

    private FdbusMediaServer createServer(String name)
    {
        FdbusMediaServer svr = new FdbusMediaServer(name);
        svr.server().setListener(svr);
        return svr;
    }

    public static void main(String[] args)
    {
        Fdbus fdbus = new Fdbus();
        MediaServer svr = new MediaServer();

        ArrayList<FdbusMediaServer> servers = new ArrayList<FdbusMediaServer>();
        for (String arg : args)
        {
            FdbusMediaServer server = svr.createServer(arg + "_server");
            server.server().bind("svc://" + arg);
            server.startBroadcast();
            
            servers.add(server);
        }

        // test dynamic behavior: bind/unbind, create/destroy
        while (true)
        {
            System.out.println("=====================bind/unbind test=========================");
            try{Thread.sleep(2500);}catch(InterruptedException e){System.out.println(e);}
            System.out.println("unbinding...");
            for (FdbusMediaServer server : servers)
            {
                server.server().unbind();
            }
            try{Thread.sleep(50);}catch(InterruptedException e){System.out.println(e);}
            System.out.println("binding...");
            for (FdbusMediaServer server : servers)
            {
                server.server().bind();
            }

            System.out.println("=====================create/destroy test=========================");
            try{Thread.sleep(2500);}catch(InterruptedException e){System.out.println(e);}
            System.out.println("destroying...");
            for (FdbusMediaServer server : servers)
            {
                server.stopBroadcast();
                server.server().destroy();
            }
            servers.clear();
            try{Thread.sleep(50);}catch(InterruptedException e){System.out.println(e);}
            System.out.println("create...");
            for (String arg : args)
            {
                FdbusMediaServer server = svr.createServer(arg + "_server");
                server.server().bind("svc://" + arg);
                server.startBroadcast();

                servers.add(server);
            }
        }

        //try{Thread.sleep(500000);}catch(InterruptedException e){System.out.println(e);}
    }
}


