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
import ipc.fdbus.FdbusMsgBuilder;

public class FdbusMessage
{
    public int returnValue()
    {
        return mStatus;
    }

    private native boolean fdb_reply(long native_handle,
                                    byte[] pb_data,
                                    String log_msg);
    private native boolean fdb_broadcast(long native_handle,
                                         int msg_code,
                                         String filter,
                                         byte[] pb_data,
                                         String log_msg);
    private native void fdb_destroy(long native_handle);
    private native boolean fdb_log_enabled(long native_handle);
    
    private void initialize(long handle,
                            int sid,
                            int msg_code,
                            String topic,
                            byte[] payload,
                            Object user_data,
                            int status)
    {
        mNativeHandle = handle;
        mSid = sid;
        mMsgCode = msg_code;
        mPayload = payload;
        mUserData = user_data;
        mTopic = topic;
        mStatus = status;
    }
    
    public FdbusMessage(int sid, int msg_code, byte[] payload, Object user_data, int status)
    {
        initialize(0, sid, msg_code, null, payload, user_data, status);
    }
    public FdbusMessage(int sid, int msg_code, String topic, byte[] payload, Object user_data, int status)
    {
        initialize(0, sid, msg_code, topic, payload, user_data, status);
    }
    public FdbusMessage(int sid, int msg_code, byte[] payload, Object user_data)
    {
        initialize(0, sid, msg_code, null, payload, user_data, Fdbus.FDB_ST_OK);
    }
    public FdbusMessage(int sid, int msg_code, byte[] payload, int status)
    {
        initialize(0, sid, msg_code, null, payload, null, status);
    }
    public FdbusMessage(int sid, int msg_code, String topic, byte[] payload, int status)
    {
        initialize(0, sid, msg_code, topic, payload, null, status);
    }
    public FdbusMessage(int sid, int msg_code, byte[] payload)
    {
        initialize(0, sid, msg_code, null, payload, null, Fdbus.FDB_ST_OK);
    }
    public FdbusMessage(long handle, int sid, int msg_code, byte[] payload)
    {
        initialize(handle, sid, msg_code, null, payload, null, Fdbus.FDB_ST_OK);
    }

    /*
     * get raw data received from remote
     */
    public byte[] byteArray()
    {
        return mPayload;
    }

    /*
     * get message id
     * message id is uniquely identify a message between client and server
     */
    public int code()
    {
        return mMsgCode;
    }
    
    /*
     * get session id
     * session id is uniquely identify a connection between client and server
     */
    public int sid()
    {
        return mSid;
    }

    /*
     * get user data from onReply()
     * The user data can be any object set at invokeAsync(). When server
     *     reply the invoke, it can be retrieved here
     */
    public Object userData()
    {
        return mUserData;
    }

    /*
     * get topic from onBroadcast() at client side
     */
    public String topic()
    {
        return mTopic;
    }

    public void topic(String tpc)
    {
        mTopic = tpc;
    }
    
    /*
     * send reply to client at onInvoke() at server side
     * @msg - the message to be reply
     */
    public boolean reply(Object msg)
    {
        FdbusMsgBuilder builder = Fdbus.encodeMessage(msg, logEnabled());
        if (builder == null)
        {
            return false;
        }

        boolean ret = fdb_reply(mNativeHandle,
                                builder.toBuffer(),
                                builder.toString());
        destroy();
        return ret;
    }

    /*
     * broadcast initial value to the client in onSubscribe()
     * @msg_code - message id
     * @topic - the topic to be broadcast
     * @msg - the message to be broadcast (protobuf format by default)
     * When client subscribe a list of events, onSubscribe() will be called
     *    at server side, in which initial value of the subscribed event
     *    should be sent to the client
     * onBroadcast() of the client will be called.
     */
    public boolean broadcast(int msg_code, String topic, Object msg)
    {
        FdbusMsgBuilder builder = Fdbus.encodeMessage(msg, logEnabled());
        if (builder == null)
        {
            return false;
        }

        boolean ret = fdb_broadcast(mNativeHandle,
                                    msg_code,
                                    topic,
                                    builder.toBuffer(),
                                    builder.toString());
        destroy();
        return ret;
    }

    /*
     * broadcast initial value to the client in onSubscribe() without topic
     */
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
    private Object mUserData;
    private String mTopic;
    private int mStatus;
}
