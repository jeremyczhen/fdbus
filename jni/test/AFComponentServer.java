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

import ipc.fdbus.Fdbus;
import ipc.fdbus.SubscribeItem;
import ipc.fdbus.FdbusDeserializer;
import ipc.fdbus.FdbusParcelable;
import ipc.fdbus.FdbusParcelable.TextFormatter;
import ipc.fdbus.FdbusAppListener;
import ipc.fdbus.FdbusAFComponent;
import ipc.fdbus.FdbusServer;
import ipc.fdbus.FdbusMessage;
import ipc.fdbus.NFdbExample;
import ipc.fdbus.Example.FdbusProtoBuilder;
import ipc.fdbus.Example.CPerson;
import ipc.fdbus.FdbusSimpleBuilder;

import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;

public class AFComponentServer
{
    private ArrayList<FdbusServer> mServerList;
    private Timer mTimer;
    private int mElapseTime;

    public AFComponentServer()
    {
        mElapseTime = 0;
    }
    private class ClientBroadcaster extends TimerTask
    {
        public void run()
        {
            for (int i = 0; i < mServerList.size(); i++)
            {
                NFdbExample.ElapseTime.Builder proto_builder = NFdbExample.ElapseTime.newBuilder();
                proto_builder.setHour(0);
                proto_builder.setMinute(0);
                proto_builder.setSecond(mElapseTime);
                NFdbExample.ElapseTime et = proto_builder.build();
                FdbusProtoBuilder builder = new FdbusProtoBuilder(et);
                FdbusServer server = mServerList.get(i);
                server.broadcast(NFdbExample.FdbMediaSvcMsgId.NTF_ELAPSE_TIME_VALUE, "my_filter", builder);
            }
            mElapseTime++;
        }
    }

    private ClientBroadcaster createBroadcaster()
    {
        return new ClientBroadcaster();
    }

    public static void main(String[] args)
    {
        Fdbus fdbus = new Fdbus();
        AFComponentServer comp_server = new AFComponentServer();

        comp_server.mServerList = new ArrayList<FdbusServer>();
        FdbusAFComponent component = new FdbusAFComponent("media server component");
        ArrayList<SubscribeItem> message_list = new ArrayList<SubscribeItem>();
        message_list.add(SubscribeItem.newAction(NFdbExample.FdbMediaSvcMsgId.REQ_METADATA_VALUE,
                new FdbusAppListener.Action(){
                    public void handleMessage(FdbusMessage msg)
                    {
                        try {
                            NFdbExample.SongId song_id = NFdbExample.SongId.parseFrom(msg.byteArray());
                            System.out.println("CP1: onInvoke: metadata request is received. song id: " + song_id.getId());
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
                        proto_builder.setElapseTime(comp_server.mElapseTime);
                        NFdbExample.NowPlayingDetails now_playing = proto_builder.build();
                        FdbusProtoBuilder builder = new FdbusProtoBuilder(now_playing);
                        msg.reply(builder);
                    }
                }));
        message_list.add(SubscribeItem.newAction(NFdbExample.FdbMediaSvcMsgId.REQ_RAWDATA_VALUE,
                new FdbusAppListener.Action(){
                    public void handleMessage(FdbusMessage msg)
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
                }));
        for (String arg : args)
        {
            FdbusServer server = component.offerService(arg, message_list,
                new FdbusAppListener.Connection(){
                    public void onConnectionStatus(int sid, boolean is_online, boolean is_first)
                    {
                        System.out.println("onConnectionStatus - session: " + sid + ", is_online: " + is_online
                                            + ", is_first: " + is_first);
                    }
                });
            comp_server.mServerList.add(server);
        }

        comp_server.mTimer = new Timer();
        comp_server.mTimer.schedule(comp_server.createBroadcaster(), 300, 300);

        try{Thread.sleep(500000);}catch(InterruptedException e){System.out.println(e);}
    }
}
