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
import ipc.fdbus.Fdbus;

public class FdbusMessage
{
    public int returnValue()
    {
        return mStatus;
    }

    private native boolean fdb_reply(long native_handle,
                                    byte[] pb_data,
                                    int encoding,
                                    String log_msg);
    private native boolean fdb_broadcast(long native_handle,
                                         int msg_code,
                                         String filter,
                                         byte[] pb_data,
                                         int encoding,
                                         String log_msg);
    private native void fdb_destroy(long native_handle);
    private native boolean fdb_log_enabled(long native_handle);
    
    private void initialize(long handle,
                            int sid,
                            int msg_code,
                            byte[] payload,
                            int encoding,
                            Object user_data,
                            int status)
    {
        mNativeHandle = handle;
        mSid = sid;
        mMsgCode = msg_code;
        mPayload = payload;
        mEncoding = encoding;
        mUserData = user_data;
        mTopic = null;
        mStatus = status;
    }
    
    public FdbusMessage(int sid, int msg_code, byte[] payload, int encoding, Object user_data, int status)
    {
        initialize(0, sid, msg_code, payload, encoding, user_data, status);
    }
    public FdbusMessage(int sid, int msg_code, byte[] payload, int encoding, Object user_data)
    {
        initialize(0, sid, msg_code, payload, encoding, user_data, Fdbus.FDB_ST_OK);
    }
    public FdbusMessage(int sid, int msg_code, byte[] payload, int encoding, int status)
    {
        initialize(0, sid, msg_code, payload, encoding, null, status);
    }
    public FdbusMessage(int sid, int msg_code, byte[] payload, int encoding)
    {
        initialize(0, sid, msg_code, payload, encoding, null, Fdbus.FDB_ST_OK);
    }
    public FdbusMessage(long handle, int sid, int msg_code, byte[] payload, int encoding)
    {
        initialize(handle, sid, msg_code, payload, encoding, null, Fdbus.FDB_ST_OK);
    }
    
    public byte[] byteArray()
    {
        return mPayload;
    }
    
    public int code()
    {
        return mMsgCode;
    }
    
    public int sid()
    {
        return mSid;
    }
    
    public Object userData()
    {
        return mUserData;
    }

    public String topic()
    {
        return mTopic;
    }

    public void topic(String tpc)
    {
        mTopic = tpc;
    }
    
    public boolean reply(byte[] pb_data)
    {
        boolean ret = fdb_reply(mNativeHandle, pb_data, Fdbus.FDB_MSG_ENC_PROTOBUF, null);
        destroy();
        return ret;
    }

    public boolean reply(Object msg)
    {
        if (Fdbus.messageParser() == null)
        {
            destroy();
            return false;
        }

        String log_data = null;
        if (logEnabled())
        {
            log_data = Fdbus.messageParser().toString(msg, Fdbus.FDB_MSG_ENC_PROTOBUF);
        }

        boolean ret = fdb_reply(mNativeHandle,
                                Fdbus.messageParser().serialize(msg, Fdbus.FDB_MSG_ENC_PROTOBUF),
                                Fdbus.FDB_MSG_ENC_PROTOBUF,
                                null);
        destroy();
        return ret;
    }
    
    public boolean broadcast(int msg_code, String topic, byte[] pb_data)
    {
        boolean ret = fdb_broadcast(mNativeHandle,
                                    msg_code,
                                    topic,
                                    pb_data,
                                    Fdbus.FDB_MSG_ENC_PROTOBUF,
                                    null);
        destroy();
        return ret;
    }
    
    public boolean broadcast(int msg_code, byte[] pb_data)
    {
        return broadcast(msg_code, null, pb_data);
    }

    public boolean broadcast(int msg_code, String topic, Object msg)
    {
        if (Fdbus.messageParser() == null)
        {
            destroy();
            return false;
        }

        String log_data = null;
        if (logEnabled())
        {
            log_data = Fdbus.messageParser().toString(msg, Fdbus.FDB_MSG_ENC_PROTOBUF);
        }

        boolean ret = fdb_broadcast(mNativeHandle,
                                    msg_code,
                                    topic,
                                    Fdbus.messageParser().serialize(msg, Fdbus.FDB_MSG_ENC_PROTOBUF),
                                    Fdbus.FDB_MSG_ENC_PROTOBUF,
                                    log_data);
        destroy();
        return ret;
    }

    public boolean broadcast(int msg_code, Object msg)
    {
        return broadcast(msg_code, null, msg);
    }

    public boolean logEnabled()
    {
        return fdb_log_enabled(mNativeHandle);
    }

    private void destroy()
    {
        long handle = mNativeHandle;
        mNativeHandle = 0;
        if (handle != 0)
        {
            fdb_destroy(handle);
        }
    }

    private long mNativeHandle;
    private int mSid;
    private int mMsgCode;
    private byte[] mPayload;
    private int mEncoding;
    private Object mUserData;
    private String mTopic;
    private int mStatus;
}
