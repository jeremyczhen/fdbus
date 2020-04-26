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
import ipc.fdbus.SubscribeItem;
import ipc.fdbus.FdbusServer;
import ipc.fdbus.FdbusClient;
import ipc.fdbus.SubscribeItem;
import ipc.fdbus.FdbusMessage;
import ipc.fdbus.FdbusMsgBuilder;

public class Fdbus
{
    /*============================================================*
     *  The following definition should keep exact the same       *
     *  as that in FDBus lib!                                     *
     *============================================================*/
    // definition for message type
    public final static int FDB_MT_UNKNOWN          = 0;
    public final static int FDB_MT_REQUEST          = 1;
    public final static int FDB_MT_REPLY            = 2;
    public final static int FDB_MT_SUBSCRIBE_REQ    = 3;
    public final static int FDB_MT_BROADCAST        = 4;
    public final static int FDB_MT_SIDEBAND_REQUEST = 5;
    public final static int FDB_MT_SIDEBAND_REPLY   = 6;
    public final static int FDB_MT_STATUS           = 7;
    public final static int FDB_MT_MAX              = 8;

    // Fdbus Message invoke Status
    public final static int FDB_ST_OK               = 0;
    public final static int FDB_ST_AUTO_REPLY_OK    = -11;
    public final static int FDB_ST_SUBSCRIBE_OK     = -12;
    public final static int FDB_ST_SUBSCRIBE_FAIL   = -13;
    public final static int FDB_ST_UNSUBSCRIBE_OK   = -14;
    public final static int FDB_ST_UNSUBSCRIBE_FAIL = -15;
    public final static int FDB_ST_TIMEOUT          = -16;
    public final static int FDB_ST_INVALID_ID       = -17;
    public final static int FDB_ST_PEER_VANISH      = -18;
    public final static int FDB_ST_DEAD_LOCK        = -19;
    public final static int FDB_ST_UNABLE_TO_SEND   = -20;
    public final static int FDB_ST_NON_EXIST        = -21;
    public final static int FDB_ST_ALREADY_EXIST    = -22;
    public final static int FDB_ST_MSG_DECODE_FAIL  = -23;
    public final static int FDB_ST_BAD_PARAMETER    = -24;
    public final static int FDB_ST_NOT_AVAILABLE    = -25;
    public final static int FDB_ST_INTERNAL_FAIL    = -26;
    public final static int FDB_ST_OUT_OF_MEMORY    = -27;
    public final static int FDB_ST_NOT_IMPLEMENTED  = -28;
    public final static int FDB_ST_OBJECT_NOT_FOUND = -29;
    public final static int FDB_ST_AUTHENTICATION_FAIL = -30;
    public final static int FDB_ST_UNKNOWN = -128;

    // Fdbus log level
    public final static int FDB_LL_VERBOSE          = 0;
    public final static int FDB_LL_DEBUG            = 1;
    public final static int FDB_LL_INFO             = 2;
    public final static int FDB_LL_WARNING          = 3;
    public final static int FDB_LL_ERROR            = 4;
    public final static int FDB_LL_FATAL            = 5;
    public final static int FDB_LL_SILENT           = 6;
    public final static int FDB_LL_MAX              = 7;

    private native void fdb_init(Class server, Class client, Class sub_item, Class msg);
    private static native void fdb_log_trace(String tag, int level, String data);
    
    public Fdbus()
    {
        System.loadLibrary("fdbus-jni");
        fdb_init(FdbusServer.class, FdbusClient.class, SubscribeItem.class, FdbusMessage.class);
    }

    public static void LOG_D(String tag, String data)
    {
        fdb_log_trace(tag, FDB_LL_DEBUG, data);
    }

    public static void LOG_I(String tag, String data)
    {
        fdb_log_trace(tag, FDB_LL_INFO, data);
    }

    public static void LOG_W(String tag, String data)
    {
        fdb_log_trace(tag, FDB_LL_WARNING, data);
    }

    public static void LOG_E(String tag, String data)
    {
        fdb_log_trace(tag, FDB_LL_ERROR, data);
    }

    public static void LOG_F(String tag, String data)
    {
        fdb_log_trace(tag, FDB_LL_FATAL, data);
    }

    public static FdbusMsgBuilder encodeMessage(Object msg, boolean enable_log)
    {
        if ((msg == null) || (msg instanceof byte[])) 
        {
            return new FdbusMsgBuilder((byte[])msg, null);
        }
        
        if (msg instanceof FdbusMsgBuilder)
        {
            FdbusMsgBuilder builder = (FdbusMsgBuilder)msg;
            if (!builder.build(enable_log))
            {
                LOG_E("fdbus-jni", "Fail to build message!\n");
                return null;
            }
            return builder;
        }
        
        LOG_E("fdbus-jni", "Unsupported message type!\n");
        return null;
    }
}

