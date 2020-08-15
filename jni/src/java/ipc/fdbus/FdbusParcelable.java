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

public class FdbusParcelable
{
    public static class TextFormatter
    {
        private final static int FDB_BYTEARRAY_PRINT_SIZE = 16;

        public TextFormatter()
        {
            mStream = new String();
        }
        public TextFormatter(String stream)
        {
            mStream = stream;
        }
        public void append(String data)
        {
            mStream += data;
        }
        public String stream()
        {
            return mStream;
        }
        private void formatOne(Object value)
        {
            if (value instanceof FdbusParcelable)
            {
                FdbusParcelable op = (FdbusParcelable)value;
                mStream += "{";
                op.toString(this);
                mStream += "}";
            }
            else if (value instanceof String)
            {
                String os = (String)value;
                mStream += os;
            }
            else if (value instanceof Integer)
            {
                Integer oi = (Integer)value;
                mStream += oi.toString();
            }
            else if (value instanceof byte[])
            {
                byte[] ob = (byte[])value;

                int len = ob.length;
                if (len > FDB_BYTEARRAY_PRINT_SIZE)
                {
                    Integer l = (Integer)len;
                    mStream += l.toString() + "[";
                    len = FDB_BYTEARRAY_PRINT_SIZE;
                }
                else
                {
                    mStream += "[";
                }
                for (int i = 0; i < len; ++i)
                {
                    Byte data = (Byte)ob[i];
                    mStream += data.toString() + ",";
                }
                mStream += "]";
            }
            else if (value instanceof byte[][])
            {
                byte[][] ob = (byte[][])value;
                mStream += "[";
                for (int i = 0; i < ob.length; ++i)
                {
                    formatOne(ob[i]);
                    mStream += ",";
                }
                mStream += "]";
            }
            else
            {
                mStream += "error!";
            }
        }
        
        public void format(Object value)
        {
            if (value instanceof Object[])
            {
                mStream += "[";
                Object[] oa = (Object[])value;
                for (int i = 0; i < oa.length; ++i)
                {
                    formatOne(oa[i]);
                    mStream += ",";
                }
                mStream += "]";
            }
            else
            {
                formatOne(value);
            }
        }
        
        public void format(String key, Object value)
        {
            mStream += key + ":";
            format(value);
            mStream += ",";
        }
        private String mStream;
    }
    
    public void serialize(FdbusSerializer serializer)
    {}
    public void deserialize(FdbusDeserializer deserializer)
    {}
    public void toString(TextFormatter fmter)
    {
        fmter.format("null");
    }
    public String toString()
    {
        TextFormatter fmter = new TextFormatter();
        fmter.format(this);
        return fmter.stream();
    }
}

