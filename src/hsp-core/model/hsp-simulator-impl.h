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
#include "ThreadPool.h"
#include "lockfree-skiplist-scheduler.h"
#include <list>

#include <atomic>
#include <vector>

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
  static void runOneNode(uint32_t context, shared_ptr<sl_map_gc<Scheduler::EventKey, EventImpl*>> evList);
  static void gc();
  
private:

  /** Next event unique id. */
  std::atomic<uint32_t> m_uid;  
  std::atomic<uint32_t> m_sub_uid;  
  /** The event count. */
  std::atomic<uint64_t> m_eventCount;
  uint32_t m_destroyCtx;

  /** 记录每个线程执行时的Context*/
	static sl_map_gc<SystemThread::ThreadId, uint32_t> m_currentCtx;
  /** 记录每个Context 的时间戳 */
	static sl_map_gc<uint32_t, uint64_t> m_currentTs;
  static sl_map_gc<uint32_t, uint32_t> m_currentUid;
  static LockFreeScheduler m_events;
  static std::list<EventId> m_destroy;

  // status
  bool m_stop;  //标记是否开始
  bool m_start; // 标记是否结束

};

}

#endif