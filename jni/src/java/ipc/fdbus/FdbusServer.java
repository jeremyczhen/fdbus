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

import ipc.fdbus.FdbusServerListener;
import ipc.fdbus.SubscribeItem;
import ipc.fdbus.Fdbus;
import ipc.fdbus.FdbusMsgBuilder;
import java.util.ArrayList;

public class FdbusServer
{
    private native long fdb_create(String name);
    private native void fdb_enable_event_cache(long native_handle, boolean enable);
    private native void fdb_destroy(long native_handle);
    private native boolean fdb_bind(long native_handle, String url);
    private native boolean fdb_unbind(long native_handle);
    private native boolean fdb_broadcast(long native_handle,
                                         int msg_code,
                                         String filter,
                                         byte[] pb_data,
                                         String log_msg);

    private native String fdb_endpoint_name(long native_handle);
    private native String fdb_bus_name(long native_handle);
    private native boolean fdb_log_enabled(long native_handle, int msg_type);

    private native void fdb_init_event_cache(long native_handle,
                                             int event,
                                             String topic,
                                             byte[] data,
                                             boolean always_update);
    
    private long mNativeHandle;
    private FdbusServerListener mFdbusListener;

    private void initialize(String name, FdbusServerListener listener)
    {
        mNativeHandle = fdb_create(name);
        mFdbusListener = listener;
    }

    /*
     * create fdbus server 
     * @name - name of the server for debugging; can be any string
     * @listener - callbacks to handle events from client 
     */
    public FdbusServer(String name, FdbusServerListener listener)
    {
        initialize(name, listener);
    }

    /*
     * create fdbus client with default name
     */
    public FdbusServer(FdbusServerListener listener)
    {
        initialize(null, listener);
    }

    public FdbusServer(String name)
    {
        initialize(name, null);
    }

    public FdbusServer()
    {
        initialize(null, null);
    }

    /*
     * set client event listener
     * @listener - callbacks to handle events from server
     */
    public void setListener(FdbusServerListener listener)
    {
        mFdbusListener = listener;
    }

    /*
     * destroy a server 
     */
    public void destroy()
    {
        long handle = mNativeHandle;
        mNativeHandle = 0;
        if (handle != 0)
        {
            fdb_destroy(handle);
        }
    }

    /*
     * bind an address
     * @url - url of server to connect in the following format:
     *     tcp://ip address:port number
     *     ipc://directory to unix domain socket
     *     svc://server name: own server name and get address dynamically
     *         allocated by name server
     */
    public boolean bind(String url)
    {
        return fdb_bind(mNativeHandle, url);
    }

    /*
     * rebind an address
     * The server should has been bound with bind(String url) then
     *     unbound with unbind()
     */
    public boolean bind()
    {
        return bind("svc://" + busName());
    }

    /*
     * release bound address
     */
    public boolean unbind()
    {
        return fdb_unbind(mNativeHandle);
    }

    /*
     * broadcast event to client
     * @msg_code - message id
     * @topic - topic of the event
     * @msg - message to be broadcasted
     * Note that only the clients subscribed the message code and topic
     *    can receive the message at onBroadcast()
     */
    public boolean broadcast(int msg_code, String topic, Object msg)
    {
        FdbusMsgBuilder builder = Fdbus.encodeMessage(msg, logEnabled(Fdbus.FDB_MT_BROADCAST));
        if (builder == null)
        {
            return false;
        }

        return fdb_broadcast(mNativeHandle,
                            msg_code,
                            topic,
                            builder.toBuffer(),
                            builder.toString());
    }

    public boolean initEventCache(int event, String topic, Object msg, boolean always_update)
    {
        FdbusMsgBuilder builder = Fdbus.encodeMessage(msg, false);
        if (builder == null)
        {
            return false;
        }

        fdb_init_event_cache(mNativeHandle, event, topic, builder.toBuffer(), always_update);
        return true;
    }

    /*
     * broadcast event to client without topic
     */
    public boolean broadcast(int msg_code, Object msg)
    {
        return broadcast(msg_code, null, msg);
    }

    /*
     * get endpoint name of the server 
     */
    public String endpointName()
    {
        return fdb_endpoint_name(mNativeHandle);
    }

    /*
     * get bus name the server is owned
     * Note that only the server bound with svc://svc_name have bus name,
     * , e.g., svc_name
     */
    public String busName()
    {
        return fdb_bus_name(mNativeHandle);
    }

    public boolean logEnabled(int msg_type)
    {
        return fdb_log_enabled(mNativeHandle, msg_type);
    }

    public void enableEventCache(boolean enable)
    {
        fdb_enable_event_cache(mNativeHandle, enable);
    }
    
    private void callbackOnline(int sid, boolean is_first)
    {
        if (mFdbusListener != null)
        {
            mFdbusListener.onOnline(sid, is_first);
        }
    }
    
    private void callbackOffline(int sid, boolean is_last)
    {
        if (mFdbusListener != null)
        {
            mFdbusListener.onOffline(sid, is_last);
        }
    }
    
    private void callbackInvoke(int sid, int msg_code, byte[] payload, long msg_handle)
    {
        if (mFdbusListener != null)
        {
            FdbusMessage msg = new FdbusMessage(msg_handle, sid, msg_code, payload);
            mFdbusListener.onInvoke(msg);
        }
    }
    
    private void callbackSubscribe(int sid, long msg_handle, ArrayList<SubscribeItem> sub_list)
    {
        if (mFdbusListener != null)
        {
            FdbusMessage msg = new FdbusMessage(msg_handle, sid, 0, null);
            mFdbusListener.onSubscribe(msg, sub_list);
        }
    }
}

