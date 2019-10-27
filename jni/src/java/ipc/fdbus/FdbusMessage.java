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

public class FdbusMessage
{
    public final static int FDB_ST_OK              = 0;
    public final static int FDB_ST_AUTO_REPLY_OK   = -11;
    public final static int FDB_ST_SUBSCRIBE_OK    = -12;
    public final static int FDB_ST_SUBSCRIBE_FAIL  = -13;
    public final static int FDB_ST_UNSUBSCRIBE_OK  = -14;
    public final static int FDB_ST_UNSUBSCRIBE_FAIL= -15;
    public final static int FDB_ST_TIMEOUT         = -16;
    public final static int FDB_ST_INVALID_ID      = -17;
    public final static int FDB_ST_PEER_VANISH     = -18;
    public final static int FDB_ST_DEAD_LOCK       = -19;
    public final static int FDB_ST_UNABLE_TO_SEND  = -20;
    public final static int FDB_ST_NON_EXIST       = -21;
    public final static int FDB_ST_ALREADY_EXIST   = -22;
    public final static int FDB_ST_MSG_DECODE_FAIL = -23;
    public final static int FDB_ST_BAD_PARAMETER   = -24;
    public final static int FDB_ST_NOT_AVAILABLE   = -25;
    public final static int FDB_ST_INTERNAL_FAIL   = -26;
    public final static int FDB_ST_OUT_OF_MEMORY   = -27;
    public final static int FDB_ST_NOT_IMPLEMENTED = -28;
    public final static int FDB_ST_OBJECT_NOT_FOUND= -29;
    public final static int FDB_ST_AUTHENTICATION_FAIL = -30;
    private int mStatus;
    public int returnValue()
    {
        return mStatus;
    }

    public void returnValue(int stat)
    {
        mStatus = stat;
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
    private void initialize(long handle,
                            int sid,
                            int msg_code,
                            byte[] payload,
                            int encoding,
                            Object user_data)
    {
        mNativeHandle = handle;
        mSid = sid;
        mMsgCode = msg_code;
        mPayload = payload;
        mEncoding = encoding;
        mUserData = user_data;
        mTopic = null;
        mStatus = FDB_ST_OK;
    }
    public FdbusMessage(int sid, int msg_code, byte[] payload, int encoding, Object user_data)
    {
        initialize(0, sid, msg_code, payload, encoding, user_data);
    }
    public FdbusMessage(int sid, int msg_code, byte[] payload, int encoding)
    {
        initialize(0, sid, msg_code, payload, encoding, null);
    }
    public FdbusMessage(long handle, int sid, int msg_code, byte[] payload, int encoding)
    {
        initialize(handle, sid, msg_code, payload, encoding, null);
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
        boolean ret = fdb_reply(mNativeHandle, pb_data, 1, null);
        destroy();
        return ret;
    }
    
    public boolean broadcast(int msg_code, String topic, byte[] pb_data)
    {
        boolean ret = fdb_broadcast(mNativeHandle, msg_code, topic, pb_data, 1, null);
        destroy();
        return ret;
    }
    
    public boolean broadcast(int msg_code, byte[] pb_data)
    {
        return fdb_broadcast(mNativeHandle, msg_code, null, pb_data, 1, null);
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
}
