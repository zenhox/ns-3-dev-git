#include "hsp-simulator-impl.h"
#include "ns3/log.h"
#include <iostream>

using namespace std;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("HSPSimulatorImpl");

NS_OBJECT_ENSURE_REGISTERED (HSPSimulatorImpl);


TypeId
HSPSimulatorImpl::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HSPSimulatorImpl")
    .SetParent<SimulatorImpl> ()
    .SetGroupName ("HSP-Core")
    .AddConstructor<HSPSimulatorImpl> ()
  ;
  return tid;
}

HSPSimulatorImpl::HSPSimulatorImpl(){
  m_stop = false;
  m_start = false;
  m_systemId = MPI_Helper::getSystemId(); 
  m_systemNm = MPI_Helper::getSystemNum();
	m_currentTs = 0;
  m_currentUid = 0;
  m_curSliceId = 0;
  m_basePtr = MPI_Helper::getBasePtr();
}

HSPSimulatorImpl::~HSPSimulatorImpl(){
  NS_LOG_FUNCTION (this);
}

void HSPSimulatorImpl::Destroy (){
  NS_LOG_FUNCTION (this);
  // while (!m_destroy.empty ()) 
  // {
  //   Ptr<EventImpl> ev = m_destroy.front ().PeekEventImpl ();
  //   m_destroy.pop_front ();
  //   NS_LOG_LOGIC ("handle destroy " << ev);
  //   if (!ev->IsCancelled ())
  //     {
  //       ev->Invoke ();
  //     }
  // }
}

bool HSPSimulatorImpl::IsFinished (void) const{
  NS_LOG_FUNCTION (this);
  return m_stop;
}

void HSPSimulatorImpl::Stop (void){
  NS_LOG_FUNCTION (this);
  m_stop = true;
}

void HSPSimulatorImpl::Stop (Time const &delay){
  NS_LOG_FUNCTION (this << delay.GetTimeStep ());
  Simulator::Schedule (delay, &Simulator::Stop);
}

//
// Schedule an event for a _relative_ time in the future.
//
EventId
HSPSimulatorImpl::Schedule (Time const &delay, EventImpl *event)
{
  NS_LOG_FUNCTION (this);
  if(m_basePtr == nullptr)
    return EventId (event, 0,0,0,0);
  MPI_GLB_DATA* g_data_ptr = (MPI_GLB_DATA*)m_basePtr;
  if(delay.GetTimeStep() != 0 && delay.GetTimeStep()  < (g_data_ptr->minDelay)->load())
    (g_data_ptr->minDelay)->store(delay.GetTimeStep()) ;
  NS_LOG_INFO ("调用了 Schedule, current context=" << GetContext()<<", delay="<< delay.GetTimeStep());
  //得到当前的context
  uint32_t context = GetContext();

  Scheduler::Event ev;

  GenEvent(context, delay, event, ev);
  
  LockFreeScheduler* scheduler = GetScheduler(context);
  scheduler->Insert(ev);

  return EventId (event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid, ev.key.m_sub_uid);
}

void 
HSPSimulatorImpl::ScheduleWithContext (uint32_t context, const Time &delay, EventImpl *event){
  NS_LOG_FUNCTION (this);
  if(!m_start && ((int)context % m_systemNm) != m_systemId)
    return;
  if(m_basePtr == nullptr)
    return ;
  MPI_GLB_DATA* g_data_ptr = (MPI_GLB_DATA*)m_basePtr;
  if(delay.GetTimeStep() != 0 && delay.GetTimeStep()  < (g_data_ptr->minDelay)->load())
    (g_data_ptr->minDelay)->store(delay.GetTimeStep()) ;
  NS_LOG_INFO ("调用了 ScheduleWithContext context="<<context<<", delay="<< delay.GetTimeStep());
  cout << "system : "<< m_systemId << "调用了 ScheduleWithContext context="<<context<<", delay="<< delay.GetTimeStep() << endl;

  Scheduler::Event ev;

  GenEvent(context, delay, event, ev);

  LockFreeScheduler* scheduler = GetScheduler(context);
  scheduler->Insert(ev);

  return;
}

 EventId HSPSimulatorImpl::ScheduleNow (EventImpl *event){
  Scheduler::Event ev;
  uint32_t context = GetContext();
  GenEvent(context, TimeStep(0), event, ev);
  LockFreeScheduler* scheduler = GetScheduler(context);
  scheduler->Insert(ev);
  return EventId (event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid, ev.key.m_sub_uid);
}
 EventId HSPSimulatorImpl::ScheduleDestroy (EventImpl *event){
  EventId id (Ptr<EventImpl> (event, false), m_currentTs, 0xffffffff, 2);
  m_destroy.push_back (id);
  return id;
}

void HSPSimulatorImpl::Remove (const EventId &id){
  NS_LOG_FUNCTION (this);
  cout << "调用了Remove" << endl;
  if (id.GetUid () == 2)
    {
      // destroy events.
      for (std::list<EventId>::iterator i = m_destroy.begin (); i != m_destroy.end (); i++)
        {
          if (*i == id)
            {
              m_destroy.erase (i);
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

  LockFreeScheduler* scheduler = GetScheduler(GetContext());
  scheduler->Remove(event);

  event.impl->Cancel ();
  // whenever we remove an event from the event list, we have to unref it.
  event.impl->Unref ();
 }

/** \copydoc Simulator::Cancel */
 void HSPSimulatorImpl::Cancel (const EventId &id){
   NS_LOG_FUNCTION (this);
   if (!IsExpired (id))
   {
      // cout <<"Cancel" << endl;
      id.PeekEventImpl ()->Cancel ();
   }
  return;
}

/** \copydoc Simulator::IsExpired */
bool HSPSimulatorImpl::IsExpired (const EventId &id) const {
  NS_LOG_FUNCTION (this);
  if (id.GetUid () == 2)
  {
      if (id.PeekEventImpl () == 0 ||
          id.PeekEventImpl ()->IsCancelled ())
        {
          return true;
        }
      // destroy events.
      for (std::list<EventId>::const_iterator i = m_destroy.begin (); i != m_destroy.end (); i++)
      {
          if (*i == id)
          {
              return false;
          }
      }
      return true;
  }
  if (id.PeekEventImpl () == 0 ||
      id.GetTs () < Now() ||
      (id.GetTs () == Now() &&
       id.GetUid () <= NowUid()) ||
      id.PeekEventImpl ()->IsCancelled ()) 
    {
      return true;
    }
  else
  {
      return false;
  }
}

/** \copydoc Simulator::Run */
 void HSPSimulatorImpl::Run (void) {
  NS_LOG_FUNCTION (this);
  m_start = true;
  if(m_systemId == 0){
    // 0 号系统 是master
    cout << "你们好" << endl;
  }
  else{
    // 其他系统
    cout << "不好" << endl;
  } 
  m_stop = true;
}

/** \copydoc Simulator::GetContext */
uint32_t HSPSimulatorImpl::GetContext (void) const {
   NS_LOG_FUNCTION (this);
   return m_systemId;
}

/** \copydoc Simulator::Now */
Time HSPSimulatorImpl::Now (void) const {
  NS_LOG_FUNCTION (this);
  return TimeStep(m_currentTs);
}

uint64_t HSPSimulatorImpl::NowTimestamp (void) const {
  return m_currentTs;
}

uint32_t HSPSimulatorImpl::NowUid()const{
  NS_LOG_FUNCTION (this);
  return m_currentUid;
}

/** \copydoc Simulator::GetDelayLeft */
Time HSPSimulatorImpl::GetDelayLeft (const EventId &id) const {
  if (IsExpired (id))
    {
      return TimeStep (0);
    }
  else
    {
      return TimeStep (id.GetTs () - NowTimestamp());
    }
}

/** \copydoc Simulator::GetMaximumSimulationTime */
 Time HSPSimulatorImpl::GetMaximumSimulationTime (void) const {
  // 暂时不实现
  return TimeStep (0x7fffffffffffffffLL);
}


/**
  * Set the Scheduler to be used to manage the event list.
  *
  * \param [in] schedulerFactory A new event scheduler factory.
  *
  * The event scheduler can be set at any time: the events scheduled
  * in the previous scheduler will be transferred to the new scheduler
  * before we start to use it.
  */
 void HSPSimulatorImpl::SetScheduler (ObjectFactory schedulerFactory) {
  //  cout << "调用了 SetScheduler" << endl;
  //暂不支持动态更换Scheduler
}

/** \copydoc Simulator::GetSystemId */
uint32_t HSPSimulatorImpl::GetSystemId () const {
   NS_LOG_FUNCTION (this);
   return m_systemId;
}

/** \copydoc Simulator::GetEventCount */
uint64_t HSPSimulatorImpl::GetEventCount (void) const {
   NS_LOG_FUNCTION (this);
  if(m_basePtr == nullptr)
    return 0;
  MPI_GLB_DATA* g_data_ptr = (MPI_GLB_DATA*)m_basePtr;
  return (g_data_ptr->eventCount)->load();
}



}

