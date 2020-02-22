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

    public int out8()
    {
        return readRaw8();
    }

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

    public int out16()
    {
        return readRawLittleEndian16();
    }

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

    public int out32()
    {
        return readRawLittleEndian32();
    }

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

    public long out64()
    {
        return readRawLittleEndian64();
    }

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

    public String outS()
    {
        return readString();
    }

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

    private <T> FdbusParcelable createParcelable(Class<T> clz)
    {
        FdbusParcelable p;
        try {
            p = (FdbusParcelable)clz.getDeclaredConstructor().newInstance();
        } catch (Exception e) {
            p = null;
        }
        return p;
    }

    public <T> FdbusParcelable out(Class<T> clz)
    {
        FdbusParcelable parcelable = createParcelable(clz);
        if (parcelable != null)
        {
            parcelable.deserialize(this);
        }
        return parcelable;
    }

    public <T> FdbusParcelable[] outA(Class<T> clz)
    {
        int len = readRawLittleEndian16();
        FdbusParcelable[] arr = new FdbusParcelable[len];
        for (int i = 0; i < len; ++i)
        {
            if (mError)
            {
                break;
            }
            FdbusParcelable p = createParcelable(clz);
            if (p == null)
            {
                break;
            }
            else
            {
                p.deserialize(this);
                arr[i] = p;
            }
        }
        return arr;
    }
}

