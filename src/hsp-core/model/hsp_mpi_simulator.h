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

#ifndef NS3_HSP_SIMULATOR_IMPL_H
#define NS3_HSP_SIMULATOR_IMPL_H

#include "ns3/simulator-impl.h"
#include "ns3/scheduler.h"
#include "ns3/event-impl.h"
#include "ns3/ptr.h"
#include "lockfree-skiplist-scheduler.h"

#include <list>

namespace ns3 {


/**
 * \ingroup simulator
 * \ingroup mpi
 *
 * \brief Distributed simulator implementation using lookahead
 */
class HspSimualtorImpl : public SimulatorImpl
{
public:
  static TypeId GetTypeId (void);

  HspSimualtorImpl ();
  ~HspSimualtorImpl ();

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
  virtual Time GetDelayLeft (const EventId &id) const;
  virtual Time GetMaximumSimulationTime (void) const;
  virtual void SetScheduler (ObjectFactory schedulerFactory);
  virtual uint32_t GetSystemId (void) const;
  virtual uint32_t GetContext (void) const;
  virtual uint64_t GetEventCount (void) const;

private:
  virtual void DoDispose (void);

  double NextTs (double) ;
  void ProcessNextTs(void);

  typedef std::list<EventId> DestroyEvents;

  DestroyEvents m_destroyEvents;

  bool m_start;
  bool m_stop;
  double m_stop_time;
 
  LockFreeScheduler m_events;

  uint32_t m_uid;
  uint32_t m_sub_uid;
  uint32_t m_currentUid;
  uint64_t m_currentTs;
  double   m_currentTslice;
  uint32_t m_currentContext;
  /** The event count. */
  uint64_t m_eventCount;

  uint32_t     m_myId;        // MPI Rank
  uint32_t     m_systemCount; // MPI Size
  bool         m_globalFinished;
};

} // namespace ns3

#endif /* NS3_DISTRIBUTED_SIMULATOR_IMPL_H */
