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
import ipc.fdbus.FdbusParcelable;

public class FdbusSerializer
{
    private static final int EXPANSION_BLOCK_SIZE = 2000;

    public FdbusSerializer()
    {
        mBufferPos = 0;
        mBuffer = new byte[EXPANSION_BLOCK_SIZE];
    }
      
    /** Read a {@code string} field value from the stream. */
    private void writeString(String string)
    {
        byte[] bytes;
        try {
            bytes = string.getBytes("UTF-8");
        } catch (Exception e) {
            bytes = new byte[0];
        }
        
        int size = bytes.length;
        writeRawLittleEndian16(size + 1);
        writeRawBytes(bytes);
        writeRaw8(0);   //end of string
    }

    /** Write an array of bytes. */
    private void writeRawBytes(final byte[] value, int length)
    {
        expandBuffer(length);
        System.arraycopy(value, 0, mBuffer, mBufferPos, length);
        mBufferPos += length;
    }

    /** Write an array of bytes. */
    private void writeRawBytes(final byte[] value)
    {
        writeRawBytes(value, value.length);
    }

    private void writeRaw8(final int value)
    {
        expandBuffer(1);
        mBuffer[mBufferPos++] = (byte)((value      ) & 0xFF);
    }

    /** Write a little-endian 32-bit integer. */
    private void writeRawLittleEndian16(final int value)
    {
        expandBuffer(2);
        mBuffer[mBufferPos++] = (byte)((value      ) & 0xFF);
        mBuffer[mBufferPos++] = (byte)((value >>  8) & 0xFF);
    }

    /** Write a little-endian 32-bit integer. */
    private void writeRawLittleEndian32(final int value)
    {
        expandBuffer(4);
        mBuffer[mBufferPos++] = (byte)((value      ) & 0xFF);
        mBuffer[mBufferPos++] = (byte)((value >>  8) & 0xFF);
        mBuffer[mBufferPos++] = (byte)((value >> 16) & 0xFF);
        mBuffer[mBufferPos++] = (byte)((value >> 24) & 0xFF);
    }

    /** Write a little-endian 64-bit integer. */
    private void writeRawLittleEndian64(final long value)
    {
        expandBuffer(8);
        mBuffer[mBufferPos++] = (byte)((value      ) & 0xFF);
        mBuffer[mBufferPos++] = (byte)((value >>  8) & 0xFF);
        mBuffer[mBufferPos++] = (byte)((value >> 16) & 0xFF);
        mBuffer[mBufferPos++] = (byte)((value >> 24) & 0xFF);
        mBuffer[mBufferPos++] = (byte)((value >> 32) & 0xFF);
        mBuffer[mBufferPos++] = (byte)((value >> 40) & 0xFF);
        mBuffer[mBufferPos++] = (byte)((value >> 48) & 0xFF);
        mBuffer[mBufferPos++] = (byte)((value >> 56) & 0xFF);
    }

    private void expandBuffer(int expected)
    {
        if ((mBufferPos + expected) <= mBuffer.length)
        {
            return;
        }
        
        int new_size = mBufferPos + expected + EXPANSION_BLOCK_SIZE;
        byte[] new_buf = new byte[new_size];
        System.arraycopy(mBuffer, 0, new_buf, 0, mBuffer.length);
        mBuffer = new_buf;
    }

    // -----------------------------------------------------------------

    private byte[] mBuffer;
    private int mBufferPos;

    public int getPosition()
    {
        return mBufferPos;
    }

    public byte[] getBuffer()
    {
        return mBuffer;
    }

    // export serialized stream to buffer
    public byte[] export()
    {
        byte[] b = new byte[mBufferPos];
        System.arraycopy(mBuffer, 0, b, 0, mBufferPos);
        return b;
    }

    // serialize one byte into stream
    public void in8(int value)
    {
        writeRaw8(value);
    }

    // serialize array of bytes into stream
    public void in8(int[] value)
    {
        writeRawLittleEndian16(value.length);
        for (int i = 0; i < value.length; ++i)
        {
            writeRaw8(value[i]);
        }
    }

    // serialize one word into stream
    public void in16(int value)
    {
        writeRawLittleEndian16(value);
    }

    // serialize array of words into stream
    public void in16(int[] value)
    {
        writeRawLittleEndian16(value.length);
        for (int i = 0; i < value.length; ++i)
        {
            writeRawLittleEndian16(value[i]);
        }
    }

    // serialize one integer into stream
    public void in32(int value)
    {
        writeRawLittleEndian32(value);
    }

    // serialize array of integer into stream
    public void in32(int[] value)
    {
        writeRawLittleEndian16(value.length);
        for (int i = 0; i < value.length; ++i)
        {
            writeRawLittleEndian32(value[i]);
        }
    }

    // serialize one long integer into stream
    public void in64(long value)
    {
        writeRawLittleEndian64(value);
    }

    // serialize array of long integers into stream
    public void in64(long[] value)
    {
        writeRawLittleEndian16(value.length);
        for (int i = 0; i < value.length; ++i)
        {
            writeRawLittleEndian64(value[i]);
        }
    }

    // serialize one boolean into stream
    public void inBool(boolean value)
    {
        writeRaw8(value ? 1 : 0);
    }

    // serialize array of booleans into stream
    public void inBool(boolean[] value)
    {
        writeRawLittleEndian16(value.length);
        for (int i = 0; i < value.length; ++i)
        {
            writeRaw8(value[i] ? 1 : 0);
        }
    }

    // serialize one string into stream
    public void inS(String value)
    {
        writeString(value);    
    }

    // serialize array of strings into stream
    public void inS(String[] value)
    {
        writeRawLittleEndian16(value.length);
        for (int i = 0; i < value.length; ++i)
        {
            writeString(value[i]);
        }
    }

    // serialize byte array (raw data)
    public void in(byte[] value, int length)
    {
        writeRawLittleEndian32(length);
        writeRawBytes(value, length);
    }

    // serialize byte array (raw data)
    public void in(byte[] value)
    {
        in(value, value.length);
    }

    // serialize array of byte array (raw data)
    public void in(byte[][] value)
    {
        writeRawLittleEndian16(value.length);
        for (int i = 0; i < value.length; ++i)
        {
            in(value[i]);
        }
    }

    /*
     * serialize one parcelable into stream
     * parcelable should be subclass of FdbusParcelable
     */
    public void in(FdbusParcelable parcelable)
    {
        parcelable.serialize(this);
    }

    /*
     * serialize array of parcelables into stream
     * elements in parcelables should be subclass of FdbusParcelable
     */
    public void in(FdbusParcelable[] parcelables)
    {
        writeRawLittleEndian16(parcelables.length);
        for (int i = 0; i < parcelables.length; ++i)
        {
            parcelables[i].serialize(this);
        }
    }
}
