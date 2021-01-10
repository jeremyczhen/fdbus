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
import ipc.fdbus.FdbusClient;
import ipc.fdbus.FdbusMessage;
import ipc.fdbus.NFdbExample;
import ipc.fdbus.Example.FdbusProtoBuilder;
import ipc.fdbus.Example.CPerson;

import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;

public class AFComponentClient
{
    private ArrayList<FdbusClient> mClientList;
    private Timer mTimer;
    private int mSongId;

    public AFComponentClient()
    {
        mSongId = 0;
    }

    private class ServerInvoker extends TimerTask
    {
        public void run()
        {
            for (int i = 0; i < mClientList.size(); i++)
            {
                NFdbExample.SongId.Builder proto_builder = NFdbExample.SongId.newBuilder();
                proto_builder.setId(mSongId);
                NFdbExample.SongId song_id = proto_builder.build();
                FdbusProtoBuilder builder = new FdbusProtoBuilder(song_id);
                FdbusClient client = mClientList.get(i);
                client.invokeAsync(NFdbExample.FdbMediaSvcMsgId.REQ_METADATA_VALUE, builder,
                    new FdbusAppListener.Action()
                    {
                        public void handleMessage(FdbusMessage msg)
                        {
                            try {
                                NFdbExample.NowPlayingDetails np =
                                        NFdbExample.NowPlayingDetails.parseFrom(msg.byteArray());
                                System.out.println(client.busName() + 
                                                   ": CP3 Reply is received - now playing info: " +
                                                   np.toString());
                            } catch (Exception e) {
                                System.out.println(e);
                            }
                        }
                    },
                    0);
                client.invokeAsync(NFdbExample.FdbMediaSvcMsgId.REQ_RAWDATA_VALUE, null, 
                    new FdbusAppListener.Action()
                    {
                        public void handleMessage(FdbusMessage msg)
                        {
                            FdbusDeserializer deserializer = new FdbusDeserializer(msg.byteArray());
                            CPerson[] persons = deserializer.out(new CPerson[deserializer.arrayLength()],
                                                                    CPerson.class);
                            TextFormatter fmter = new TextFormatter();
                            fmter.format(persons);
                            System.out.println(client.busName() + ": CP4 Reply is received - " + fmter.stream());
                        }
                    },
                    0);
            }
            mSongId++;
        }
    }

    private ServerInvoker createInvoker()
    {
        return new ServerInvoker();
    }

    public static void main(String[] args)
    {
        Fdbus fdbus = new Fdbus();
        AFComponentClient comp_client = new AFComponentClient();

        comp_client.mClientList = new ArrayList<FdbusClient>();
        FdbusAFComponent component = new FdbusAFComponent("media client component");
        for (String arg : args)
        {
            ArrayList<SubscribeItem> event_list = new ArrayList<SubscribeItem>();
            event_list.add(SubscribeItem.newAction(NFdbExample.FdbMediaSvcMsgId.NTF_ELAPSE_TIME_VALUE, "my_filter",
                    new FdbusAppListener.Action(){
                        public void handleMessage(FdbusMessage msg)
                        {
                            try {
                                NFdbExample.ElapseTime et =
                                        NFdbExample.ElapseTime.parseFrom(msg.byteArray());
                                System.out.println(arg + ": CP1: elapse time is received. topic: " + msg.topic() +
                                                    ", code: " + msg.code() + ", value: " + et.toString());
                            } catch (Exception e) {
                                System.out.println(e);
                            }
                        }
                    }));
            event_list.add(SubscribeItem.newAction(NFdbExample.FdbMediaSvcMsgId.NTF_ELAPSE_TIME_VALUE, "raw_buffer",
                    new FdbusAppListener.Action(){
                        public void handleMessage(FdbusMessage msg)
                        {
                            System.out.println(arg + ": CP2: elapse time is received. topic: " + msg.topic() +
                                                ", code: " + msg.code());
                        }
                    }));
            FdbusClient client = component.queryService(arg, event_list,
                new FdbusAppListener.Connection()
                {
                    public void onConnectionStatus(int sid, boolean is_online, boolean is_first)
                    {
                        System.out.println(arg + ": onConnectionStatus - session: " + sid + ", is_online: " + is_online
                                            + ", is_first: " + is_first);
                    }
                });
            comp_client.mClientList.add(client);
        }

        comp_client.mTimer = new Timer();
        comp_client.mTimer.schedule(comp_client.createInvoker(), 300, 300);

        try{Thread.sleep(500000);}catch(InterruptedException e){System.out.println(e);}
    }
}

