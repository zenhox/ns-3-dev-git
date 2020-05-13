/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * A time slice based parallel simulator.
 * Author: Hox Zheng
 */

#ifndef NS3_HSP_SIMULATOR_IMPL_H
#define NS3_HSP_SIMULATOR_IMPL_H

#include "ns3/simulator-impl.h"
#include "ns3/scheduler.h"
#include "ns3/simulator.h"
#include "ns3/system-thread.h"
#include "sl_map.h"
#include "lockfree-skiplist-scheduler.h"
#include <list>

#include <atomic>
#include <vector>
#include "mpi-helper.h"

namespace ns3 {

/**
 * \ingroup simulator
 * \ingroup mpi
 *
 * \brief Distributed simulator implementation using lookahead
 */
class HSPSimulatorImpl : public SimulatorImpl
{
public:
  /**
   *  Register this type.
   *  \return The object TypeId.
   */
  static TypeId GetTypeId (void);
  
  HSPSimulatorImpl ();
  ~HSPSimulatorImpl ();

  // virtual from SimulatorImpl
  virtual void Destroy ();
  virtual bool IsFinished (void) const;
  virtual void Stop (void);
  virtual void Stop (Time const &delay);
  virtual EventId Schedule (Time const &delay, EventImpl *event);
  virtual void ScheduleWithContext (uint32_t context, Time const &delay, EventImpl *event);
  virtual EventId ScheduleNow (EventImpl *event);
  virtual EventId ScheduleDestroy (EventImpl *event);
  virtual void Remove (const EventId &id);
  virtual void Cancel (const EventId &id);
  virtual bool IsExpired (const EventId &id) const;
  virtual void Run (void);
  virtual Time Now (void) const;
  virtual uint64_t NowTimestamp (void) const;
  virtual uint32_t NowUid()const;
  virtual Time GetDelayLeft (const EventId &id) const;
  virtual Time GetMaximumSimulationTime (void) const;
  virtual void SetScheduler (ObjectFactory schedulerFactory);
  virtual uint32_t GetSystemId (void) const;
  virtual uint32_t GetContext (void) const;
  virtual uint64_t GetEventCount (void) const;
private:
  
  inline LockFreeScheduler* GetScheduler(uint32_t context);
  inline void WriteState(int state);
  inline double ReadGlbCurrSlice();
  inline void WriteGlbNextSlice(double slice);
  inline void WaitAllDone(); /*for rank 0: read all status*/
  inline void GenEvent(uint32_t context, Time const &delay, EventImpl *event, Scheduler::Event &ev);

  int m_systemId; 
  int m_systemNm;
  /*LP info*/
	uint64_t m_currentTs;
  uint32_t m_currentUid;
  double m_curSliceId;
  bool m_stop;  //标记是否开始
  bool m_start; // 标记是否结束
  
  
  std::list<EventId> m_destroy; /*for rank 0*/

  void * m_basePtr;
};


inline LockFreeScheduler* HSPSimulatorImpl::GetScheduler(uint32_t context){
  if(m_basePtr == nullptr)
    return nullptr;
  MPI_GLB_DATA*  g_data_ptr = (MPI_GLB_DATA*)m_basePtr;
  return (g_data_ptr->schedulers)[context];
}

inline void HSPSimulatorImpl::WriteState(int state){
  if(m_basePtr == nullptr)
    return ;
  MPI_GLB_DATA* g_data_ptr = (MPI_GLB_DATA*)m_basePtr;
  ((g_data_ptr->g_status)[m_systemId])->store(state);
}

inline double HSPSimulatorImpl::ReadGlbCurrSlice(){
  if(m_basePtr == nullptr)
    return -1;
  MPI_GLB_DATA* g_data_ptr = (MPI_GLB_DATA*)m_basePtr;
  return (g_data_ptr->g_currTimeSlice)->load();  
}

inline void HSPSimulatorImpl::WriteGlbNextSlice(double slice){
  if(m_basePtr == nullptr)
    return;
  MPI_GLB_DATA* g_data_ptr = (MPI_GLB_DATA*)m_basePtr;
  (g_data_ptr->g_nextTimeSlice)->store(slice); 
  return;
}

/*for rank 0: read all status*/
inline void HSPSimulatorImpl::WaitAllDone() 
{
  if(m_basePtr == nullptr)
    return;
  MPI_GLB_DATA* g_data_ptr = (MPI_GLB_DATA*)m_basePtr;
  for(int i=0; i < MPI_Helper::getSystemNum(); ++i){
    int state = ((g_data_ptr->g_status)[i])->load();
    while(state == 0){
      state = ((g_data_ptr->g_status)[i])->load();
    }
  }
}

inline void HSPSimulatorImpl::GenEvent(uint32_t context, Time const &delay, EventImpl *event, Scheduler::Event &ev){
  // NS_LOG_INFO ("调用了 ScheduleWithContext context="<<context<<", delay="<< delay.GetTimeStep());
  if(m_basePtr == nullptr)
    return ;
  MPI_GLB_DATA* g_data_ptr = (MPI_GLB_DATA*)m_basePtr;
  if(delay.GetTimeStep() != 0 && delay.GetTimeStep()  < (g_data_ptr->minDelay)->load())
    g_data_ptr->minDelay->store(delay.GetTimeStep());
  uint64_t currentTs = m_currentTs;
  Time tAbsolute = delay + TimeStep (currentTs);

  Scheduler::Event new_ev;
  new_ev.impl = event;
  new_ev.key.m_ts = static_cast<uint64_t> (tAbsolute.GetTimeStep ());
  new_ev.key.m_context = context;
  if(m_start && delay.GetTimeStep() == 0) //当前时间片立即插入
  {
    new_ev.key.m_uid = m_currentUid;
    new_ev.key.m_sub_uid = (g_data_ptr->sub_uid)->load();
    (*(g_data_ptr->sub_uid))++;
  }
  else{
    new_ev.key.m_uid = (g_data_ptr->uid)->load();
    new_ev.key.m_sub_uid = 0;
    (*(g_data_ptr->uid))++;
  }
  (*(g_data_ptr->eventCount))++;
  ev = new_ev;
  return;
}

}

#endif