#include "hsp-simulator-impl.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("HSPSimulatorImpl");

NS_OBJECT_ENSURE_REGISTERED (HSPSimulatorImpl);

size_t getIndexND(size_t nd){
    if(nd > 10)
        return 3;
    else if(nd > 6)
        return 2;
    else if(nd > 1)
        return 1;
    else 
        return 0;
}
size_t getIndexEV(uint64_t ev){
    if(ev > 100)
        return 3;
    else if(ev > 50)
        return 2;
    else if(ev > 10)
        return 1;
    else 
        return 0; 
}

LockFreeScheduler HSPSimulatorImpl::m_events;
/** 记录每个Context 的时间戳 */
sl_map_gc<uint32_t, uint64_t> HSPSimulatorImpl::m_currentTs;
std::list<EventId> HSPSimulatorImpl::m_destroy;
sl_map_gc<SystemThread::ThreadId, uint32_t> HSPSimulatorImpl::m_currentCtx;
sl_map_gc<uint32_t, uint32_t> HSPSimulatorImpl::m_currentUid;

std::atomic<uint64_t> g_exeCnt(0);
 
void HSPSimulatorImpl::runOneNode(uint32_t context, shared_ptr<sl_map_gc<Scheduler::EventKey, EventImpl*>> evList){
  // cout << "执行context："<< context << endl;
  NS_LOG_INFO("执行context："<< context);
  m_currentCtx.insert(std::make_pair(SystemThread::Self(), context));
  // 要更换当前线程的context
  m_currentCtx.find(SystemThread::Self())->second = context;
  for(auto it = evList->begin(); it != evList->end(); ++it){
    auto itr = m_currentTs.find(context);
    itr->second = (it->first).m_ts;
    auto itr2 = m_currentUid.find(context);
    itr2->second = (it->first).m_uid;
    (it->second)->Invoke();
    (it->second)->Unref ();
    g_exeCnt++;
  }
}

void HSPSimulatorImpl::gc(){
   m_events.gc();
}

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
  m_initCount = 0;
  m_eventCount.store(0);
  m_uid.store(4);
  m_sub_uid.store(1);
}

HSPSimulatorImpl::~HSPSimulatorImpl(){
  NS_LOG_FUNCTION (this);
}

void HSPSimulatorImpl::Destroy (){
  NS_LOG_FUNCTION (this);
  while (!m_destroy.empty ()) 
  {
    Ptr<EventImpl> ev = m_destroy.front ().PeekEventImpl ();
    m_destroy.pop_front ();
    NS_LOG_LOGIC ("handle destroy " << ev);
    if (!ev->IsCancelled ())
      {
        ev->Invoke ();
      }
  }
}

bool HSPSimulatorImpl::IsFinished (void) const{
  NS_LOG_FUNCTION (this);
  return m_stop;
}


void HSPSimulatorImpl::Stop (void){
  NS_LOG_FUNCTION (this);
  cout <<"调用了Stop" << endl;
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
  NS_LOG_INFO ("调用了 Schedule, current context=" << GetContext()<<", delay="<< delay.GetTimeStep());
  // NS_LOG_FUNCTION (this << delay.GetTimeStep () << event);

  //得到当前的context
  uint32_t context = GetContext();

  //插入这个context的时间戳(sl map在已经存在时会忽略这条指令)
  m_currentTs.insert(std::make_pair(context, 0));
  m_currentUid.insert(std::make_pair(context, 0));
  
  uint64_t currentTs = m_currentTs.find(context)->second;
  Time tAbsolute = delay + TimeStep (currentTs);

  // NS_ASSERT (tAbsolute.IsPositive ());
  // NS_ASSERT (tAbsolute >= TimeStep (m_currentTs));

  Scheduler::Event ev;
  ev.impl = event;
  ev.key.m_ts = static_cast<uint64_t> (tAbsolute.GetTimeStep ());
  ev.key.m_context = context;
  if(m_start &&  delay.GetTimeStep() == 0) //当前时间片立即插入
  {
    ev.key.m_uid = NowUid();
    ev.key.m_sub_uid = m_sub_uid.load();
    m_sub_uid++;
  }
  else{
    ev.key.m_uid = m_uid;
    ev.key.m_sub_uid = 0;
    m_uid++;
  }
  m_events.Insert (ev);
  m_eventCount++;
  return EventId (event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid, ev.key.m_sub_uid);
}

void 
HSPSimulatorImpl::ScheduleWithContext (uint32_t context, const Time &delay, EventImpl *event){
  NS_LOG_FUNCTION (this);
  // cout << "调用了 ScheduleWithContext context="<<context<<", delay="<< delay.GetTimeStep()<< endl; 
  NS_LOG_INFO ("调用了 ScheduleWithContext context="<<context<<", delay="<< delay.GetTimeStep());
  //插入这个context的时间戳(sl map在已经存在时会忽略这条指令)
  m_currentTs.insert(std::make_pair(context, 0));  
  m_currentUid.insert(std::make_pair(context, 0));

  if(!m_start){
    m_initCount++;
  }

  uint64_t currentTs = m_currentTs.find(GetContext())->second;
  Time tAbsolute = delay + TimeStep (currentTs);

  // NS_ASSERT (tAbsolute.IsPositive ());
  // NS_ASSERT (tAbsolute >= TimeStep (m_currentTs));

  Scheduler::Event ev;
  ev.impl = event;
  ev.key.m_ts = static_cast<uint64_t> (tAbsolute.GetTimeStep ());
  ev.key.m_context = context;
  if(m_start && delay.GetTimeStep() == 0) //当前时间片立即插入
  {
    ev.key.m_uid = NowUid();
    ev.key.m_sub_uid = 1;
  }
  else{
    ev.key.m_uid = m_uid;
    ev.key.m_sub_uid = 0;
    m_uid++;
  }
  m_events.Insert (ev);
  m_eventCount++;
  return;
}

 EventId HSPSimulatorImpl::ScheduleNow (EventImpl *event){
  Scheduler::Event ev;
  ev.impl = event;
  ev.key.m_ts = NowTimestamp();
  ev.key.m_context = GetContext ();
  ev.key.m_uid = NowUid();
  ev.key.m_sub_uid = m_sub_uid.load();
  m_events.Insert (ev);
  m_sub_uid++;
  return EventId (event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid, ev.key.m_sub_uid);
  // return Schedule(TimeStep(0), event);
  // return EventId(event, 0,0,0,0);
}
 EventId HSPSimulatorImpl::ScheduleDestroy (EventImpl *event){
  EventId id (Ptr<EventImpl> (event, false), NowTimestamp(), 0xffffffff, 2);
  m_destroy.push_back (id);
  m_uid++;
  // m_eventCount++;
  return id;
}

void HSPSimulatorImpl::Remove (const EventId &id){
  NS_LOG_FUNCTION (this);
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
  m_events.Remove (event);
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
  int threadNum = 4;
  shared_ptr<SliceEvents> sliceEvents; 
  std::vector< std::future<void> > results;
  results.reserve(100);
  int nodeStatis[4]=  {0};    //1, 2~6, 7~10, 11~, 
  int evetStatis[4] = {0};    //0~10, 11 ~ 50, 51 ~ 100, 100~

  ThreadPool pool(threadNum);
  m_start = true;
  while(!m_events.PeekNextSlice(sliceEvents) && !m_stop){
      auto& events = sliceEvents->_sliceEvs;
      uint64_t evCnt = sliceEvents->getEventCount();
      size_t ndCnt = events.size();
      nodeStatis[getIndexND(ndCnt)]++;
      evetStatis[getIndexEV(evCnt)]++;
      results.clear();
      if(g_exeCnt.load() <= m_initCount){
        for(auto it = events.begin(); it != events.end(); ++it){
            runOneNode( it->first, it->second);
        }
      }else{
        for(auto it = events.begin(); it != events.end(); ++it){
            results.emplace_back(pool.enqueue(runOneNode, it->first, it->second));
        }
        for(size_t i=0; i<results.size(); ++i){
            results[i].wait(); 
        }
      }
      // if(count % 100000 == 0){
      //     pool.enqueue(gc);
      // }
  }
  m_stop = true;
  cout << "Insert :" << m_eventCount.load()<<endl;
  cout << "Execun :" << m_events.getEventCount() <<endl;
  // //输出统计结果
  // for(auto i=0; i<4; ++i){
  //     cout << nodeStatis[i] << "    ";
  // }
  // cout << endl;
  // for(auto i=0; i<4; ++i){
  //     cout << evetStatis[i] << "    ";
  // }
  // cout << endl;
}

/** \copydoc Simulator::GetContext */
uint32_t HSPSimulatorImpl::GetContext (void) const {
   NS_LOG_FUNCTION (this);
   if(m_stop)
     return 0xffffffff;
   m_currentCtx.insert(std::make_pair(SystemThread::Self(), 0xffffffff));
   auto itr = m_currentCtx.find(SystemThread::Self());  
   return itr->second;
}

/** \copydoc Simulator::Now */
Time HSPSimulatorImpl::Now (void) const {
  NS_LOG_FUNCTION (this);
  if(m_stop)
    return TimeStep(m_currentTs.begin()->second);
  m_currentTs.insert(std::make_pair(GetContext(), 0));
  auto itr = m_currentTs.find(GetContext());
  return TimeStep(itr->second);
}
uint64_t HSPSimulatorImpl::NowTimestamp (void) const {
  NS_LOG_FUNCTION (this);
  if(m_stop)
    return m_currentTs.begin()->second;
  m_currentTs.insert(std::make_pair(GetContext(), 0));
  auto itr = m_currentTs.find(GetContext());
  return itr->second;
}
uint32_t HSPSimulatorImpl::NowUid()const{
  NS_LOG_FUNCTION (this);
  if(m_stop)
    return m_currentUid.begin()->second;
  m_currentUid.insert(std::make_pair(GetContext(), 0));
  auto itr = m_currentUid.find(GetContext());
  return itr->second;
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
   return 0;
}



/** \copydoc Simulator::GetEventCount */
uint64_t HSPSimulatorImpl::GetEventCount (void) const {
   NS_LOG_FUNCTION (this);
   return m_eventCount.load();
}



}

