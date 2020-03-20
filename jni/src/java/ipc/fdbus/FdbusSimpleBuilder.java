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
import ipc.fdbus.FdbusMsgBuilder;
import ipc.fdbus.FdbusParcelable.TextFormatter;

public class FdbusSimpleBuilder extends FdbusMsgBuilder
{
    public FdbusSimpleBuilder(Object msg)
    {
        super(msg);
    }
    public boolean build(boolean to_text)
    {
        FdbusSerializer serializer = new FdbusSerializer();
        if (mMsg instanceof FdbusParcelable)
        {
            FdbusParcelable msg = (FdbusParcelable)mMsg;
            serializer.in(msg);
        }
        else if (mMsg instanceof FdbusParcelable[])
        {
            FdbusParcelable[] msg = (FdbusParcelable[])mMsg;
            serializer.in(msg);
        }
        else if (mMsg instanceof String)
        {
            String msg = (String)mMsg;
            serializer.inS(msg);
        }
        else if (mMsg instanceof String[])
        {
            String[] msg = (String[])mMsg;
            serializer.inS(msg);
        }
        else
        {
            return false;
        }

        mStream = serializer.export();
        if (to_text)
        {
            TextFormatter fmter = new TextFormatter();
            fmter.format(mMsg);
            mText = fmter.stream();
        }
        
        return true;
    }
}

