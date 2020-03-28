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
import java.lang.reflect.ParameterizedType;

public class FdbusDeserializer
{
    public FdbusDeserializer(final byte[] buffer)
    {
        mBuffer = buffer;
        mBufferPos = 0;
        mError = false;
    }

  // -----------------------------------------------------------------

  /** Read a {@code double} field value from the stream. */
    private double readDouble()
    {
        return Double.longBitsToDouble(readRawLittleEndian64());
    }

    /** Read a {@code float} field value from the stream. */
    private float readFloat()
    {
        return Float.intBitsToFloat(readRawLittleEndian32());
    }

    /** Read a {@code string} field value from the stream. */
    private String readString()
    {
        final int size = readRawLittleEndian16();
        if (((mBufferPos + size) <= mBuffer.length) && (size > 0))
        {
            if (mBuffer[mBufferPos + size - 1] == 0)
            {
                final String result = new String(mBuffer, mBufferPos, size - 1);
                mBufferPos += size;
                return result;
            }
        }

        final String result = new String();
        mError = true;
        return result;
    }

    private void readRawBytes(byte[] value)
    {
        if ((mBufferPos + value.length) > mBuffer.length)
        {
            mError = true;
            return;
        }

        System.arraycopy(mBuffer, mBufferPos, value, 0, value.length);
        mBufferPos += value.length;
    }

    private int readRaw8()
    {
        if ((mBufferPos + 1) > mBuffer.length)
        {
            mError = true;
            return 0;
        }
        return mBuffer[mBufferPos++];
    }

    private int readRawLittleEndian16()
    {
        if ((mBufferPos + 2) > mBuffer.length)
        {
            mError = true;
            return 0;
        }
        final byte b1 = mBuffer[mBufferPos++];
        final byte b2 = mBuffer[mBufferPos++];
        return ((b1 & 0xff)      ) |
                ((b2 & 0xff) <<  8);
    }

    private int readRawLittleEndian32()
    {
        if ((mBufferPos + 4) > mBuffer.length)
        {
            mError = true;
            return 0;
        }
        final byte b1 = mBuffer[mBufferPos++];
        final byte b2 = mBuffer[mBufferPos++];
        final byte b3 = mBuffer[mBufferPos++];
        final byte b4 = mBuffer[mBufferPos++];
        return ((b1 & 0xff)      ) |
               ((b2 & 0xff) <<  8) |
               ((b3 & 0xff) << 16) |
               ((b4 & 0xff) << 24);
    }

    private long readRawLittleEndian64()
    {
        if ((mBufferPos + 8) > mBuffer.length)
        {
            mError = true;
            return 0;
        }
        final byte b1 = mBuffer[mBufferPos++];
        final byte b2 = mBuffer[mBufferPos++];
        final byte b3 = mBuffer[mBufferPos++];
        final byte b4 = mBuffer[mBufferPos++];
        final byte b5 = mBuffer[mBufferPos++];
        final byte b6 = mBuffer[mBufferPos++];
        final byte b7 = mBuffer[mBufferPos++];
        final byte b8 = mBuffer[mBufferPos++];
        return (((long)b1 & 0xff)      ) |
               (((long)b2 & 0xff) <<  8) |
               (((long)b3 & 0xff) << 16) |
               (((long)b4 & 0xff) << 24) |
               (((long)b5 & 0xff) << 32) |
               (((long)b6 & 0xff) << 40) |
               (((long)b7 & 0xff) << 48) |
               (((long)b8 & 0xff) << 56);
    }

    private <T> T createParcelable(Class<T> clz)
    {
        T p;
        try {
            p = (T)clz.getDeclaredConstructor().newInstance();
        } catch (Exception e) {
            System.out.println(e);
            p = null;
        }
        return p;
    }

    // -----------------------------------------------------------------

    private final byte[] mBuffer;
    private int mBufferPos;
    private boolean mError;

    public boolean isAtEnd()
    {
        return mBufferPos == mBuffer.length;
    }

    public int getPosition()
    {
        return mBufferPos;
    }

    public byte[] getBuffer()
    {
        return mBuffer;
    }

    // deserialize one byte from stream
    public int out8()
    {
        return readRaw8();
    }

    // deserialize array of bytes from stream
    public int[] out8A()
    {
        int len = readRawLittleEndian16();
        int[] arr = new int[len];
        for (int i = 0; i < len; ++i)
        {
            int data = readRaw8();
            if (mError)
            {
                break;
            }
            arr[i] = data;
        }
        return arr;
    }

    // deserialize one word from stream
    public int out16()
    {
        return readRawLittleEndian16();
    }

    // deserialize array of words from stream
    public int[] out16A()
    {
        int len = readRawLittleEndian16();
        int[] arr = new int[len];
        for (int i = 0; i < len; ++i)
        {
            int data = readRawLittleEndian16();
            if (mError)
            {
                break;
            }
            arr[i] = data;
        }
        return arr;
    }

    // deserialize one integer from stream
    public int out32()
    {
        return readRawLittleEndian32();
    }

    // deserialize array of integers from stream
    public int[] out32A()
    {
        int len = readRawLittleEndian16();
        int[] arr = new int[len];
        for (int i = 0; i < len; ++i)
        {
            int data = readRawLittleEndian32();
            if (mError)
            {
                break;
            }
            arr[i] = data;
        }
        return arr;
    }

    // deserialize one long integer from stream
    public long out64()
    {
        return readRawLittleEndian64();
    }

    // deserialize array of long integers from stream
    public long[] out64A()
    {
        int len = readRawLittleEndian16();
        long[] arr = new long[len];
        for (int i = 0; i < len; ++i)
        {
            long data = readRawLittleEndian64();
            if (mError)
            {
                break;
            }
            arr[i] = data;
        }
        return arr;
    }

    // deserialize one boolean from stream
    public boolean outBool()
    {
        return readRaw8() != 0;
    }

    // deserialize array of booleans from stream
    public boolean[] outBoolA()
    {
        int len = readRawLittleEndian16();
        boolean[] arr = new boolean[len];
        for (int i = 0; i < len; ++i)
        {
            boolean data = readRaw8() != 0;
            if (mError)
            {
                break;
            }
            arr[i] = data;
        }
        return arr;
    }

    // deserialize one string from stream
    public String outS()
    {
        return readString();
    }

    // deserialize array of strings from stream
    public String[] outSA()
    {
        int len = readRawLittleEndian16();
        String[] arr = new String[len];
        for (int i = 0; i < len; ++i)
        {
            String data = readString();
            if (mError)
            {
                break;
            }
            arr[i] = data;
        }
        return arr;
    }

    // deserialize byte array (raw data) from stream
    public byte[] outBA()
    {
        if (mError)
        {
            return new byte[0];
        }
        int len = readRawLittleEndian32();
        byte[] value = null;
        try {
            value = new byte[len];
        } catch (Exception e) {
        }
        if (value == null)
        {
            value = new byte[0];
        }
        else
        {
            readRawBytes(value);
        }
        return value;
    }

    // deserialize array of byte array (raw data) from stream
    public byte[][] outBAA()
    {
        if (mError)
        {
            return new byte[0][];
        }
        int len = readRawLittleEndian16();
        byte[][] value = null;
        try {
            value = new byte[len][];
        } catch (Exception e) {
        }
        if (value == null)
        {
            value = new byte[0][];
        }
        else
        {
            for (int i = 0; i < len; ++i)
            {
                value[i] = outBA();
            }
        }

        return value;
    }

    /*
     * deserialize one parcelable from stream; 
     *     T should be subclass of FdbusParcelable;
     *     type should be subclass_of_FdbusParcelable.class
     */
    public <T> FdbusParcelable out(Class<T> type)
    {
        FdbusParcelable parcelable = (FdbusParcelable)createParcelable(type);
        if (parcelable != null)
        {
            parcelable.deserialize(this);
        }
        return parcelable;
    }

    // get size of parcelable array
    public int arrayLength()
    {
        return readRawLittleEndian16();
    }

    /*
     * deserialize one parcelable from stream; 
     *     T should be subclass of FdbusParcelable;
     *     type should be subclass_of_FdbusParcelable.class
     *
     * example:
     * class MyMsg implements FdbusParcelable
     * {
     * }
     * public void onInvoke(FdbusMessage msg)
     * {
     *     FdbusDeserializer deserializer = new FdbusDeserializer(msg.byteArray());
     *     MyMsg[] msg_data = deserializer.out(new MyMsg[deserializer.arrayLength()], MyMsg.class);
     * }
     */
    public <T> T[] out(T[] parcelables, Class<T> type)
    {
        for (int i = 0; i < parcelables.length; ++i)
        {
            T new_obj = createParcelable(type);
            if (new_obj == null)
            {
                break;
            }
            else
            {
                FdbusParcelable p = (FdbusParcelable)new_obj;
                p.deserialize(this);
                parcelables[i] = new_obj;
            }
        }
        return parcelables;
    }
}

