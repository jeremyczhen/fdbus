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
    private void writeRawBytes(final byte[] value)
    {
        expandBuffer(value.length);
        System.arraycopy(value, 0, mBuffer, mBufferPos, value.length);
        mBufferPos += value.length;
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

    public byte[] export()
    {
        byte[] b = new byte[mBufferPos];
        System.arraycopy(mBuffer, 0, b, 0, mBufferPos);
        return b;
    }

    public void in8(int value)
    {
        writeRaw8(value);
    }

    public void in8(int[] value)
    {
        writeRawLittleEndian16(value.length);
        for (int i = 0; i < value.length; ++i)
        {
            writeRaw8(value[i]);
        }
    }

    public void in16(int value)
    {
        writeRawLittleEndian16(value);
    }

    public void in16(int[] value)
    {
        writeRawLittleEndian16(value.length);
        for (int i = 0; i < value.length; ++i)
        {
            writeRawLittleEndian16(value[i]);
        }
    }

    public void in32(int value)
    {
        writeRawLittleEndian32(value);
    }

    public void in32(int[] value)
    {
        writeRawLittleEndian16(value.length);
        for (int i = 0; i < value.length; ++i)
        {
            writeRawLittleEndian32(value[i]);
        }
    }

    public void in64(long value)
    {
        writeRawLittleEndian64(value);
    }

    public void in64(long[] value)
    {
        writeRawLittleEndian16(value.length);
        for (int i = 0; i < value.length; ++i)
        {
            writeRawLittleEndian64(value[i]);
        }
    }

    public void inS(String value)
    {
        writeString(value);    
    }

    public void inS(String[] value)
    {
        writeRawLittleEndian16(value.length);
        for (int i = 0; i < value.length; ++i)
        {
            writeString(value[i]);
        }
    }

    public void in(FdbusParcelable parcelable)
    {
        parcelable.serialize(this);
    }
    
    public void in(FdbusParcelable[] parcelables)
    {
        writeRawLittleEndian16(parcelables.length);
        for (int i = 0; i < parcelables.length; ++i)
        {
            parcelables[i].serialize(this);
        }
    }
}
