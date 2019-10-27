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
import ipc.fdbus.FdbusClientListener;
import ipc.fdbus.SubscribeItem;

import java.util.ArrayList;

//enum EFdbMessageEncoding
//{
//    FDB_MSG_ENC_RAW,
//    FDB_MSG_ENC_PROTOBUF,
//    FDB_MSG_ENC_CUSTOM1,
//    FDB_MSG_ENC_CUSTOM2,
//    FDB_MSG_ENC_CUSTOM3,
//    FDB_MSG_ENC_CUSTOM4,
//    FDB_MSG_ENC_CUSTOM5,
//    FDB_MSG_ENC_CUSTOM6,
//}

public class FdbusClient
{
    private native long fdb_create(String name);
    private native void fdb_destroy(long native_handle);
    private native boolean fdb_connect(long native_handle, String url);
    private native boolean fdb_disconnect(long native_handle);
    private native boolean fdb_invoke_async(long native_handle,
                                            int msg_code,
                                            byte[] pb_data,
                                            int encoding,
                                            String log_msg,
                                            Object user_data,
                                            int timeout);
    private native boolean fdb_send(long native_handle,
                                    int msg_code,
                                    byte[] pb_data,
                                    int encoding,
                                    String log_msg);
    private native boolean fdb_subscribe(long native_handle, 
                                         ArrayList<SubscribeItem> sub_list);
    private native boolean fdb_unsubscribe(long native_handle,
                                           ArrayList<SubscribeItem> sub_list);
    
    private native String fdb_endpoint_name(long native_handle);
    private native String fdb_bus_name(long native_handle);
	
    private long mNativeHandle;
    private FdbusClientListener mFdbusListener;

    private void initialize(String name, FdbusClientListener listener)
    {
        mNativeHandle = fdb_create(name);
        mFdbusListener = listener;
    }
    
    public FdbusClient(String name, FdbusClientListener listener)
    {
        initialize(name, listener);
    }

    public FdbusClient(FdbusClientListener listener)
    {
        initialize(null, listener);
    }

    public FdbusClient(String name)
    {
        initialize(name, null);
    }

    public FdbusClient()
    {
        initialize(null, null);
    }

    public void setListener(FdbusClientListener listener)
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

    public boolean connect(String url)
    {
        return fdb_connect(mNativeHandle, url);
    }
	
    public boolean disconnect()
    {
        return fdb_disconnect(mNativeHandle);
    }
	
    //public boolean invokeAsync(int msg_code, AbstractMessageLite msg, Object user_data, int timeout)
    //{
    //    return fdb_invoke_async(mNativeHandle, msg_code, msg.toByteArray(), 1, null, user_data, timeout);
    //}

    public boolean invokeAsync(int msg_code, byte[] pb_data, Object user_data, int timeout)
    {
        return fdb_invoke_async(mNativeHandle, msg_code, pb_data, 1, null, user_data, timeout);
    }
	
    public boolean send(int msg_code, byte[] pb_data)
    {
        return fdb_send(mNativeHandle, msg_code, pb_data, 1, null);
    }
	
    public boolean subscribe(ArrayList<SubscribeItem> sub_list)
    {
        return fdb_subscribe(mNativeHandle, sub_list);
    }
	
    public boolean unsubscribe(ArrayList<SubscribeItem> sub_list)
    {
        return fdb_unsubscribe(mNativeHandle, sub_list);
    }

    public String endpointName()
    {
        return fdb_endpoint_name(mNativeHandle);
    }

    public String busName()
    {
        return fdb_bus_name(mNativeHandle);
    }

    private void callbackOnline(int sid)
    {
        if (mFdbusListener != null)
        {
            mFdbusListener.onOnline(sid);
        }
    }
    
    private void callbackOffline(int sid)
    {
        if (mFdbusListener != null)
        {
            mFdbusListener.onOffline(sid);
        }
    }
    
    private void callbackReply(int sid,
                               int msg_code,
                               byte[] payload,
                               int encoding,
                               int status,
                               Object user_data)
    {
        if (mFdbusListener != null)
        {
            FdbusMessage msg = new FdbusMessage(sid, msg_code, payload, encoding, user_data);
            msg.returnValue(status);
            mFdbusListener.onReply(msg);
        }
    }
    
    private void callbackBroadcast(int sid,
                                   int msg_code,
                                   String filter,
                                   byte[] payload,
                                   int encoding)
    {   
        if (mFdbusListener != null)
        {
            FdbusMessage msg = new FdbusMessage(sid, msg_code, payload, encoding);
            msg.topic(filter);
            mFdbusListener.onBroadcast(msg);
        }
    }
}

