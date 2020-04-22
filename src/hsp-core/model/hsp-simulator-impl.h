/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * A time slice based parallel simulator.
 * Author: Hox Zheng
 */

#ifndef NS3_HSP_SIMULATOR_IMPL_H
#define NS3_HSP_SIMULATOR_IMPL_H

#include "ns3/simulator-impl.h"
#include "ns3/scheduler.h"
#include "ns3/ptr.h"

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
  virtual Time GetDelayLeft (const EventId &id) const;
  virtual Time GetMaximumSimulationTime (void) const;
  virtual void SetMaximumLookAhead (const Time lookAhead);
  virtual void SetScheduler (ObjectFactory schedulerFactory);
  virtual uint32_t GetSystemId (void) const;
  virtual uint32_t GetContext (void) const;
  virtual uint64_t GetEventCount (void) const;

private:

};

}

