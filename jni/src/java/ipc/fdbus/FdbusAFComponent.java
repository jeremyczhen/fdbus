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

package ipc.fdbus;
import ipc.fdbus.FdbusClient;
import ipc.fdbus.FdbusServer;
import java.util.ArrayList;
import ipc.fdbus.FdbusAppListener;

public class FdbusAFComponent
{
    private native long fdb_create(String name);
    private native long fdb_query_service(long handle,
                                          String bus_name,
                                          ArrayList<SubscribeItem> evt_handle_tbl,
                                          FdbusAppListener.Connection conn_callback);
    private native long fdb_offer_service(long handle,
                                          String bus_name,
                                          ArrayList<SubscribeItem> msg_handle_tbl,
                                          FdbusAppListener.Connection conn_callback);

    private long mNativeHandle;

    public FdbusAFComponent(String name)
    {
        mNativeHandle = fdb_create(name);
    }
    public FdbusClient queryService(String bus_name,
                                    ArrayList<SubscribeItem> evt_handle_tbl,
                                    FdbusAppListener.Connection conn_callback)
    {
        return new FdbusClient(fdb_query_service(mNativeHandle, bus_name, evt_handle_tbl, conn_callback));
    }
    public FdbusServer offerService(String bus_name,
                                    ArrayList<SubscribeItem> msg_handle_tbl,
                                    FdbusAppListener.Connection conn_callback)
    {
        return new FdbusServer(fdb_offer_service(mNativeHandle, bus_name, msg_handle_tbl, conn_callback));
    }
}
