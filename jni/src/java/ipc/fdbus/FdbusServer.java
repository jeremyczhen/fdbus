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
import java.util.ArrayList;

public class FdbusServer
{
    private native long fdb_create(String name);
    private native void fdb_destroy(long native_handle);
    private native boolean fdb_bind(long native_handle, String url);
    private native boolean fdb_unbind(long native_handle);
    private native boolean fdb_broadcast(long native_handle,
                                         int msg_code,
                                         String filter,
                                         byte[] pb_data,
                                         int encoding,
                                         String log_msg);

    private native String fdb_endpoint_name(long native_handle);
    private native String fdb_bus_name(long native_handle);
    private native boolean fdb_log_enabled(long native_handle, int msg_type);
    
    private long mNativeHandle;
    private FdbusServerListener mFdbusListener;

    private void initialize(String name, FdbusServerListener listener)
    {
        mNativeHandle = fdb_create(name);
        mFdbusListener = listener;
    }
    
    public FdbusServer(String name, FdbusServerListener listener)
    {
        initialize(name, listener);
    }

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

    public void setListener(FdbusServerListener listener)
    {
        mFdbusListener = listener;
    }

    public void destroy()
    {
        long handle = mNativeHandle;
        mNativeHandle = 0;
        if (handle != 0)
        {
            fdb_destroy(handle);
        }
    }

    public boolean bind(String url)
    {
        return fdb_bind(mNativeHandle, url);
    }

    public boolean unbind()
    {
        return fdb_unbind(mNativeHandle);
    }

    public boolean broadcast(int msg_code, String topic, byte[] pb_data)
    {
        return fdb_broadcast(mNativeHandle,
                            msg_code,
                            topic,
                            pb_data,
                            Fdbus.FDB_MSG_ENC_PROTOBUF,
                            null);
    }

    public boolean broadcast(int msg_code, byte[] pb_data)
    {
        return fdb_broadcast(mNativeHandle,
                             msg_code,
                             null,
                             pb_data,
                             Fdbus.FDB_MSG_ENC_PROTOBUF,
                             null);
    }

    public boolean broadcast(int msg_code, String topic, Object msg)
    {
        if (Fdbus.messageParser() == null)
        {
            return false;
        }

        String log_data = null;
        if (logEnabled(Fdbus.FDB_MT_BROADCAST))
        {
            log_data = Fdbus.messageParser().toString(msg, Fdbus.FDB_MSG_ENC_PROTOBUF);
        }

        return fdb_broadcast(mNativeHandle,
                            msg_code,
                            topic,
                            Fdbus.messageParser().serialize(msg, Fdbus.FDB_MSG_ENC_PROTOBUF),
                            Fdbus.FDB_MSG_ENC_PROTOBUF,
                            log_data);
    }

    public boolean broadcast(int msg_code, Object msg)
    {
        return broadcast(msg_code, null, msg);
    }

    public String endpointName()
    {
        return fdb_endpoint_name(mNativeHandle);
    }

    public String busName()
    {
        return fdb_bus_name(mNativeHandle);
    }

    public boolean logEnabled(int msg_type)
    {
        return fdb_log_enabled(mNativeHandle, msg_type);
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
    
    private void callbackInvoke(int sid, int msg_code, byte[] payload, int encoding, long msg_handle)
    {
        if (mFdbusListener != null)
        {
            FdbusMessage msg = new FdbusMessage(msg_handle, sid, msg_code, payload, encoding);
            mFdbusListener.onInvoke(msg);
        }
    }
    
    private void callbackSubscribe(int sid, long msg_handle, ArrayList<SubscribeItem> sub_list)
    {
        if (mFdbusListener != null)
        {
            FdbusMessage msg = new FdbusMessage(msg_handle, sid, 0, null, 0);
            mFdbusListener.onSubscribe(msg, sub_list);
        }
    }
}

