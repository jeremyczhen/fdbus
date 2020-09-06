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

public interface FdbusClientListener
{
    /*
     * called when a client is connected with a server
     * @sid - session id of the connection
     */
    public void onOnline(int sid);

    /*
     * called when a client is disconnected with a server
     * @sid - session id of the connection
     */
    public void onOffline(int sid);

    /*
     * called when reply is received from server
     * @msg - the message replied from server
     */
    public void onReply(FdbusMessage msg);

    /*
     * called when event is received from server (via getAsync()/getSync())
     * @msg - the message replied from server
     */
    public void onGetEvent(FdbusMessage msg);

    /*
     * called when event is broadcasted from server
     * @msg - the message broadcasted by server
     */
    public void onBroadcast(FdbusMessage msg);
}
