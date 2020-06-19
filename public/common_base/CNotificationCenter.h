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

#ifndef _CNOTIFICATIONCENTER_
#define _CNOTIFICATIONCENTER_

#include <map>
#include <set>
#include <mutex>
#include "common_defs.h"
#include "CBaseNotification.h"
#include "CBaseWorker.h"
#include "CMethodJob.h"

template<typename T>
class CBaseNotificationCenter
{
private:
    typedef std::set<typename CBaseNotification<T>::Ptr > tNotifications;
    typedef std::map< FdbEventCode_t, tNotifications > tNotificationTbl;

public:
    virtual ~CBaseNotificationCenter()
    {
    }

    /*
     * Subscribe to an event so that CBaseNotification::run() will be called event happen.
     * The event is given by CBaseNotification::event(). 'notification' is created like
     *      CBaseNotification::Ptr notification(new CBaseNotification())
     * 
     * The notification is owned by CBaseNotificationCenter until you unsubscribe it.
     * return 'true' if successfully subscribed.
     */
    bool subscribe(typename CBaseNotification<T>::Ptr &notification)
    {
        if (!onSubscribe(notification))
        {
            return false;
        }
        
        std::pair<typename tNotifications::iterator, bool> ret;
        std::lock_guard<std::mutex> _l(mMutex);
        ret = mNtfTbl[notification->event()].insert(notification);
        
        return ret.second;
    }
    
    /*
     * Unsubscribe an event: CBaseNotification::run() will no long run and 'notification'
     * is released by CBaseNotificationCenter.
     */
    void unsubscribe(typename CBaseNotification<T>::Ptr &notification)
    {
        FdbEventCode_t event = notification->event();
        std::lock_guard<std::mutex> _l(mMutex);
    	mNtfTbl[event].erase(notification);
        if (mNtfTbl[event].empty())
        {
            mNtfTbl.erase(event);
        }
    }
    
    /*
     * Notify specified event happen upon specifc notification.  CBaseNotification::run()
     *  of the notifications will be called with data as paremeter. Typically the function
     *  is triggerred by onSubscribe().
     */
    void notify(T &data, typename CBaseNotification<T>::Ptr &notification)
    {
        CBaseWorker *worker = notification->worker();
        if (worker)
        {
            worker->sendAsync(new onNotifyJob(this, notification, data));
        }
        else
        {
            notification->run(data);
        }
    }
    
    /*
     * Notify specified event happen. If the event is subscribed, CBaseNotification::run() of
     * the notifications will be called with data as paremeter.
     */
    void notify(FdbEventCode_t event, T &data)
    {
        if (mNtfTbl.empty())
        {
            return;
        }

    	std::vector<typename CBaseNotification<T>::Ptr> ntf_vec;
    	
        {
            std::lock_guard<std::mutex> _l(mMutex);
            if (mNtfTbl.empty())
            {
                return;
            }
            typename tNotificationTbl::iterator its = mNtfTbl.find(event);
            if (its != mNtfTbl.end())
            {
                    tNotifications &ntf_set = its->second;
                    for (typename tNotifications::iterator it = ntf_set.begin();
                           it != ntf_set.end(); ++it)
                    {
                            ntf_vec.push_back(*it);
                    }
            }
        }

    	for (typename std::vector<typename CBaseNotification<T>::Ptr>::iterator it = ntf_vec.begin();
    			it != ntf_vec.end(); ++it)
    	{
    	    notify(data, *it);
    	}
    }

    void notify(T &data)
    {
        notify(FDB_INVALID_ID, data);
    }

protected:
    /*
     * Called when subscribe() is called. Typically subclass can verify event and
     * notify initial value here.
     *  Note that it is called inside subscribe(). You should be aware of thread
     *  context of this function and notify() (if called).
     */
    virtual bool onSubscribe(typename CBaseNotification<T>::Ptr &notification)
    {
        return true;
    }
    
private:
    void callNotify(CBaseWorker *worker,
                    CMethodJob< CBaseNotificationCenter<T> > *job,
                    CBaseJob::Ptr &ref)
    {
        onNotifyJob *the_job = fdb_dynamic_cast_if_available<onNotifyJob *>(job);
        if (the_job)
        {
            the_job->mNotification->run(the_job->mData);
        }
    }

    class onNotifyJob : public CMethodJob< CBaseNotificationCenter<T> >
    {
    public:
        onNotifyJob(CBaseNotificationCenter *nc, typename CBaseNotification<T>::Ptr &nt, T &data)
            : CMethodJob< CBaseNotificationCenter<T> >(nc, &CBaseNotificationCenter::callNotify)
            , mNotification(nt)
            , mData(data)
        {
        }

        typename CBaseNotification<T>::Ptr mNotification;
        T mData;
    };
    
    tNotificationTbl mNtfTbl;
    std::mutex mMutex;
};

typedef CBaseNotificationCenter<void *> CGenericNotificationCenter;

#endif
