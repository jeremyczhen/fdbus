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

package ipc.fdbus.Example;
import ipc.fdbus.FdbusMsgBuilder;
import com.google.protobuf.AbstractMessageLite;
//import com.google.protobuf.nano.MessageNano;
import ipc.fdbus.FdbusSerializer;
import ipc.fdbus.FdbusParcelable;

public class FdbusProtoBuilder extends FdbusMsgBuilder
{
    public FdbusProtoBuilder(Object msg)
    {
        super(msg);
    }
    public boolean build(boolean to_text)
    {
        if (mMsg instanceof AbstractMessageLite)
        {
            AbstractMessageLite msg = (AbstractMessageLite)mMsg;
       	    mStream = msg.toByteArray();
            if (to_text)
            {
                mText = msg.toString();
            }
        }
        //else if (mMsg instanceof MessageNano)
        //{
        //    MessageNano msg = (MessageNano)mMsg;
        //    mStream = MessageNano.toByteArray((MessageNano) msg);
        //    if (to_text)
        //    {
        //        mText = msg.toString();
        //    }
        //}
        else
        {
            return false;
        }
        
        return true;
    }
}

