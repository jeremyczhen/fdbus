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

public class SubscribeItem
{
    public SubscribeItem(int code, String topic)
    {
        mCode = code;
        mTopic = topic;
    }

    public static SubscribeItem newEvent(int code, String topic)
    {
        return new SubscribeItem(code, topic);
    }

    public static SubscribeItem newEvent(int code)
    {
        return new SubscribeItem(code, null);
    }

    public static SubscribeItem newGroup(int group, String topic)
    {
        return new SubscribeItem(((group & 0xff) << 24) | 0xffffff, topic);
    }

    public static SubscribeItem newGroup(int group)
    {
        return new SubscribeItem(((group & 0xff) << 24) | 0xffffff, null);
    }

    public int code()
    {
        return mCode;
    }

    public String topic()
    {
        return mTopic;
    }

    private int mCode;
    private String mTopic;
}
