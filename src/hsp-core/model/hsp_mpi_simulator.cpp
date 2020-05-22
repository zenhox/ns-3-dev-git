/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: George Riley <riley@ece.gatech.edu>
 *
 */

#include "hsp_mpi_simulator.h"
#include "hsp_mpi_interface.h"
#include "ns3/mpi-interface.h"
#include "ns3/simulator.h"
#include "ns3/scheduler.h"
#include "ns3/event-impl.h"
#include "ns3/channel.h"
#include "ns3/node-container.h"
#include "ns3/ptr.h"
#include "ns3/pointer.h"
#include "ns3/assert.h"
#include "ns3/log.h"


#include <cmath>
#include <thread>

#include <iostream>
using namespace std;

#ifdef NS3_MPI
#include <mpi.h>
#endif

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("HspSimualtorImpl");

NS_OBJECT_ENSURE_REGISTERED (HspSimualtorImpl);

std::atomic<bool> HspSimualtorImpl::m_globalFinished(false);
std::atomic<bool> HspSimualtorImpl::m_globalStart(false);
std::atomic<uint64_t> HspSimualtorImpl::m_globalSliceCnt(0);
std::atomic<int64_t> HspSimualtorImpl::m_minDelay(9999999999);


void 
HspSimualtorImpl::message_work()
{
     while( ! m_globalStart.load()  );
     //cout << "Start receive message..." << endl;
     while( ! m_globalFinished.load() )
     {
         HspMpiInterface::ReceiveMessages (false);
     }
}

inline void message_once()
{
         HspMpiInterface::ReceiveMessages (false);
         HspMpiInterface::TestSendComplete ();
         HspMpiInterface::ReceiveMessages (false);
}
inline void gc_once(LockFreeScheduler* data, int count)
{
	data->gc(count - 100);
}

void 
HspSimualtorImpl::gc_work(LockFreeScheduler* data, int count)
{
    while( ! m_globalStart.load()  );
    cout << "Start gc..." << endl;
    while( ! m_globalFinished.load() )
    {
        uint64_t cnt = m_globalSliceCnt.load();
        if(cnt % count == 0)
        {
            //cout << "Gc once..." << endl;
            data->gc(count-100);
        }
    }
}



TypeId
HspSimualtorImpl::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HspSimualtorImpl")
    .SetParent<SimulatorImpl> ()
    .SetGroupName ("Hsp-Core")
    .AddConstructor<HspSimualtorImpl> ()
  ;
  return tid;
}

HspSimualtorImpl::HspSimualtorImpl ()
{
  NS_LOG_FUNCTION (this);

#ifdef NS3_MPI
  m_myId = MpiInterface::GetSystemId ();
  m_systemCount = MpiInterface::GetSize ();

#else
  NS_UNUSED (m_systemCount);
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif

  m_start = false;
  m_stop = false;

  // uids are allocated from 4.
  // uid 0 is "invalid" events
  // uid 1 is "now" events
  // uid 2 is "destroy" events
  m_uid = 4;
  m_sub_uid = 1;
  // before ::Run is entered, the m_currentUid will be zero
  m_currentUid = 0;
  m_currentTs = 0;
  m_currentTslice = -1;
  m_currentContext = Simulator::NO_CONTEXT;
  m_eventCount = 0;
}

HspSimualtorImpl::~HspSimualtorImpl ()
{
  NS_LOG_FUNCTION (this);
}

void
HspSimualtorImpl::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

//   while (!m_events.IsEmpty ())
//     {
//       Scheduler::Event next = m_events->RemoveNext ();
//       next.impl->Unref ();
//     }
//   m_events = 0;
//   delete [] m_pLBTS;

  SimulatorImpl::DoDispose ();
}

void
HspSimualtorImpl::Destroy ()
{
  NS_LOG_FUNCTION (this);

  while (!m_destroyEvents.empty ())
    {
      Ptr<EventImpl> ev = m_destroyEvents.front ().PeekEventImpl ();
      m_destroyEvents.pop_front ();
      NS_LOG_LOGIC ("handle destroy " << ev);
      if (!ev->IsCancelled ())
        {
          ev->Invoke ();
        }
    }

  MpiInterface::Destroy ();
}


void
HspSimualtorImpl::SetScheduler (ObjectFactory schedulerFactory)
{
  NS_LOG_FUNCTION (this << schedulerFactory);
}


bool
HspSimualtorImpl::IsFinished (void) const
{
  return m_globalFinished.load();
}


int64_t
HspSimualtorImpl::NextTs (int64_t currTs) 
{
  // If local MPI task is has no more events or stop was called
  // next event time is infinity.
  return m_events.ReadNext(currTs);
}

void
HspSimualtorImpl::Run (void)
{
  NS_LOG_FUNCTION (this);

#ifdef NS3_MPI
    if(m_myId == 0){
        cout << "Master 启动!" << endl;
        m_stop = false;
        m_start = true;
        m_globalStart.store(true);
        while(! m_stop ){
            int64_t currTs = HspMpiInterface::GetCurrTs();
            //if(currTs == m_stop_time)
            //    break;
            // \status 
            // -(s+1) 正在执行时间片s
            //   s+1  s时间片执行完毕
            unsigned doneCnt = 0;
            for(unsigned i=1; i< m_systemCount; ++i){
                int64_t state = HspMpiInterface::GetStatus(i);
                // cout << "得到status = " << state << endl;
                if( state == currTs + 1)
                  doneCnt++;
            }
            if(doneCnt == m_systemCount-1)
            {
                int64_t nextTs = HspMpiInterface::GetMinNextTs();
                int64_t currTs = HspMpiInterface::GetCurrTs();
                 //cout << "process id ="<<m_myId<<",可以继续了, nextTs="<<nextTs << ", currTs="<<currTs<<endl;
                if( nextTs == -1 || (currTs == nextTs) || (currTs == m_stop_time))  // 所有进程的下一个时间片都是-1, 可以停止了
		{
                    m_stop = true;
		    break;
		}
                HspMpiInterface::SetCurrTs(nextTs);
            }
        }
        cout << "Master: Global finished!"<<endl;
        HspMpiInterface::SetCurrTs(-1);
        m_globalFinished.store(true);
    }
    else{
        cout << "Slave:"<<m_myId<<" 启动!" << endl;
        m_stop = false;
        m_start = true;
	//std::thread message_thread(message_work);
	//std::thread gc_thread(gc_work, &m_events, 20000);
        m_globalStart.store(true);
        while(! m_stop ){
            //HspMpiInterface::TestSendComplete ();
	    message_once();
            int64_t currTs = HspMpiInterface::GetCurrTs();
            int64_t nextTs = HspMpiInterface::GetNextTs(m_myId);
            int64_t nextEventTs = NextTs(m_currentTslice);
	    if(nextTs != nextEventTs)
                HspMpiInterface::SetNextTs(m_myId, nextEventTs);
            if( currTs == -1)
                m_stop = true;
            else if( nextEventTs == currTs ){                     // 符合时间片
                m_globalSliceCnt++;
		if(m_globalSliceCnt.load() % 50000 == 0)
		{
		    gc_once(&m_events, 50000);
		}
                m_currentTslice = nextEventTs;
                HspMpiInterface::SetStatus(m_myId, -(currTs+1));  //修改为正在执行状态
                ProcessNextTs();
                int64_t tempNextTs = NextTs(m_currentTslice);
                HspMpiInterface::SetNextTs(m_myId, tempNextTs);
                HspMpiInterface::SetStatus(m_myId, currTs+1);     // 修改为执行完毕
            }
            else {
              // 这里有两种情况
              // 1 当前时间片没有事件
              // 2 master还没有更新新的时间片
              HspMpiInterface::SetStatus(m_myId, currTs+1);    
            }
        }
        m_globalFinished.store(true);
	//cout << "currTs = " <<  HspMpiInterface::GetCurrTs() << endl;
	if(m_systemCount == 2)
	{
	    cout << "Slave " << m_myId << ": min delay = "<< m_minDelay.load() << endl;
	}
        //message_thread.join();
	//gc_thread.join();
	}
#else
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif
}

uint32_t HspSimualtorImpl::GetSystemId () const
{
  return m_myId;
}

void
HspSimualtorImpl::Stop (void)
{
  NS_LOG_FUNCTION (this);
  m_stop = true;
}

void
HspSimualtorImpl::Stop (Time const &delay)
{
  NS_LOG_FUNCTION (this << delay.GetTimeStep ());
  if(m_myId == 0)
  {
    int64_t currTs = HspMpiInterface::GetCurrTs();
	  m_stop_time = m_events.calcSlice(delay) + currTs;
  }
  Simulator::Schedule (delay, &Simulator::Stop);
}

//
// Schedule an event for a _relative_ time in the future.
//
EventId
HspSimualtorImpl::Schedule (Time const &delay, EventImpl *event)
{
  NS_LOG_FUNCTION (this << delay.GetTimeStep () << event);

  Time tAbsolute = delay + TimeStep (m_currentTs);

  NS_ASSERT (tAbsolute.IsPositive ());
  NS_ASSERT (tAbsolute >= TimeStep (m_currentTs));
  Scheduler::Event ev;
  ev.impl = event;
  ev.key.m_ts = static_cast<uint64_t> (tAbsolute.GetTimeStep ());
  ev.key.m_context = GetContext ();

  if(delay.GetTimeStep() > 0)
  {
	  if(delay.GetTimeStep() < m_minDelay.load())
	  {
		  m_minDelay.store(delay.GetTimeStep());
	  }
  }

   if(m_start && delay.GetTimeStep() == 0) //当前时间片立即插入
  {
    ev.key.m_uid = m_currentUid;
    ev.key.m_sub_uid = m_sub_uid;
    m_sub_uid++;
  }
  else{
    ev.key.m_uid = m_uid;
    ev.key.m_sub_uid = 0;
    m_uid++;
  }
  m_events.Insert (ev);
  return EventId (event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid, ev.key.m_sub_uid);
}

void
HspSimualtorImpl::ScheduleWithContext (uint32_t context, Time const &delay, EventImpl *event)
{
  NS_LOG_FUNCTION (this << context << delay.GetTimeStep () << m_currentTs << event);
  // cout << m_myId << " 调用了ScheduleWithContext, contex= "<< context <<", delay=" << delay.GetTimeStep() << endl;
  Scheduler::Event ev;
  ev.impl = event;
  ev.key.m_ts = m_currentTs + delay.GetTimeStep ();
  ev.key.m_context = context;
  if(delay.GetTimeStep() > 0)
  {
	  if(delay.GetTimeStep() < m_minDelay.load())
	  {
		  m_minDelay.store(delay.GetTimeStep());
	  }
  }

  if(m_start && delay.GetTimeStep() == 0) //当前时间片立即插入
  {
    ev.key.m_uid = m_currentUid;
    ev.key.m_sub_uid = m_sub_uid;
    m_sub_uid++;
  }
  else{
    ev.key.m_uid = m_uid;
    ev.key.m_sub_uid = 0;
    m_uid++;
  }
  m_events.Insert (ev);
}

EventId
HspSimualtorImpl::ScheduleNow (EventImpl *event)
{
  NS_LOG_FUNCTION (this << event);

  Scheduler::Event ev;
  ev.impl = event;
  ev.key.m_ts = m_currentTs;
  ev.key.m_context = GetContext ();
  ev.key.m_uid = m_currentUid;
  ev.key.m_sub_uid = m_sub_uid;
  m_sub_uid++;
  m_events.Insert (ev);
  return EventId (event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid, ev.key.m_sub_uid);
}

EventId
HspSimualtorImpl::ScheduleDestroy (EventImpl *event)
{
  NS_LOG_FUNCTION (this << event);

  EventId id (Ptr<EventImpl> (event, false), m_currentTs, 0xffffffff, 2);
  m_destroyEvents.push_back (id);
  m_uid++;
  return id;
}

Time
HspSimualtorImpl::Now (void) const
{
  return TimeStep (m_currentTs);
}

Time
HspSimualtorImpl::GetDelayLeft (const EventId &id) const
{
  if (IsExpired (id))
    {
      return TimeStep (0);
    }
  else
    {
      return TimeStep (id.GetTs () - m_currentTs);
    }
}

void
HspSimualtorImpl::Remove (const EventId &id)
{
  if (id.GetUid () == 2)
    {
      // destroy events.
      for (DestroyEvents::iterator i = m_destroyEvents.begin (); i != m_destroyEvents.end (); i++)
        {
          if (*i == id)
            {
              m_destroyEvents.erase (i);
              break;
            }
        }
      return;
    }
  if (IsExpired (id))
    {
      return;
    }
  Scheduler::Event event;
  event.impl = id.PeekEventImpl ();
  event.key.m_ts = id.GetTs ();
  event.key.m_context = id.GetContext ();
  event.key.m_uid = id.GetUid ();
  m_events.Remove (event);
  event.impl->Cancel ();
  // whenever we remove an event from the event list, we have to unref it.
  event.impl->Unref ();

}

void
HspSimualtorImpl::Cancel (const EventId &id)
{
  if (!IsExpired (id))
    {
      id.PeekEventImpl ()->Cancel ();
    }
}

bool
HspSimualtorImpl::IsExpired (const EventId &id) const
{
  if (id.GetUid () == 2)
    {
      if (id.PeekEventImpl () == 0
          || id.PeekEventImpl ()->IsCancelled ())
        {
          return true;
        }
      // destroy events.
      for (DestroyEvents::const_iterator i = m_destroyEvents.begin (); i != m_destroyEvents.end (); i++)
        {
          if (*i == id)
            {
              return false;
            }
        }
      return true;
    }
  if (id.PeekEventImpl () == 0
      || id.GetTs () < m_currentTs
      || (id.GetTs () == m_currentTs
          && id.GetUid () <= m_currentUid)
      || id.PeekEventImpl ()->IsCancelled ())
    {
      return true;
    }
  else
    {
      return false;
    }
}

Time
HspSimualtorImpl::GetMaximumSimulationTime (void) const
{
  /// \todo I am fairly certain other compilers use other non-standard
  /// post-fixes to indicate 64 bit constants.
  return TimeStep (0x7fffffffffffffffLL);
}

uint32_t
HspSimualtorImpl::GetContext (void) const
{
  return m_currentContext;
}

uint64_t
HspSimualtorImpl::GetEventCount (void) const
{
  return m_eventCount;
}


void HspSimualtorImpl::ProcessNextTs(void){
    shared_ptr<EventsMap> events;
    m_events.PeekNext(events);
    for(auto it = events->begin(); it != events->end(); ++it){
        m_currentTs = (it->first).m_ts;
        m_currentUid = (it->first).m_uid;
        m_currentContext = (it->first).m_context;
        (it->second)->Invoke();
        (it->second)->Unref ();
        m_eventCount++;
    }
}

} // namespace ns3
