/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * A skip list based lock-free event scheduler.
 * Author: Hox Zheng
 */

#ifndef LOCKFREE_SKIPLIST_SCHEDULER_H
#define LOCKFREE_SKIPLIST_SCHEDULER_H

#include "scheduler.h"
#include <stdint.h>
#include <utility>

/**
 * \file
 * \ingroup scheduler
 * ns3::lockfree-skiplist-scheduler declaration.
 */

namespace ns3 {

/**
 * \ingroup scheduler
 * \brief a lockfree-skiplist event scheduler
 *
 * This class implements the an event scheduler using an lockfree-skiplist
 * data structure.
 */
class LockFreeScheduler : public Scheduler
{
public:
  /**
   *  Register this type.
   *  \return The object TypeId.
   */
  static TypeId GetTypeId (void);

  /** Constructor. */
  LockFreeScheduler ();
  /** Destructor. */
  virtual ~LockFreeScheduler ();

  // Inherited
  virtual void Insert (const Scheduler::Event &ev);
  virtual bool IsEmpty (void) const;
  virtual Scheduler::Event PeekNext (void) const;
  virtual Scheduler::Event RemoveNext (void);
  virtual void Remove (const Scheduler::Event &ev);

private:

};

} // namespace ns3

#endif /* LOCKFREE_SKIPLIST_SCHEDULER_H */
