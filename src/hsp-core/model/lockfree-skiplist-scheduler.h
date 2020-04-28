/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * A skip list based lock-free event scheduler.
 * Author: Hox Zheng
 */

#ifndef LOCKFREE_SKIPLIST_SCHEDULER_H
#define LOCKFREE_SKIPLIST_SCHEDULER_H

#include <stdint.h>
#include <utility>
#include <atomic>

#include "sl_map.h"
#include "ns3/scheduler.h"
#include "ns3/nstime.h"
#include <iostream>
using std::cout;
using std::endl;

#include <memory>
using std::shared_ptr;

/**
 * \file
 * \ingroup scheduler
 * ns3::lockfree-skiplist-scheduler declaration.
 */

namespace ns3 {

  class SliceEvents{
  public:
      // @in id : slice id
      SliceEvents(int64x64_t id);
      ~SliceEvents();

      // struct EventKey
      // {
      //   uint64_t m_ts;         /**< Event time stamp. */
      //   uint32_t m_uid;        /**< Event unique id. */
      //   uint32_t m_context;    /**< Event context. */
      // };
      // struct Event
      // {
      //   EventImpl *impl;       /**< Pointer to the event implementation. */
      //   EventKey key;          /**< Key for sorting and ordering Events. */
      // };

      inline int insertEvent(const Scheduler::Event &ev);

      uint64_t getEventCount()const;
      int64x64_t getSliceId()const;

      sl_map_gc<uint32_t, std::shared_ptr<sl_map_gc<Scheduler::EventKey, EventImpl*>>> _sliceEvs;
      
  private:
      int64x64_t _sliceId;
      std::atomic<uint64_t> _eventCnt;
  } ;

inline int SliceEvents::insertEvent(const Scheduler::Event &ev){

    uint32_t nodeId = ev.key.m_context;
    auto nevents = std::make_shared<sl_map_gc<Scheduler::EventKey, EventImpl*>>();

    // 直接插入，已经有了会被忽略
    _sliceEvs.insert(std::make_pair(nodeId, nevents));
    auto itr = _sliceEvs.find(nodeId);

    auto re = (itr->second)->insert(std::make_pair(ev.key, ev.impl));
    if(re.second  ==  false)
      cout <<"插入失败"<<endl;
    _eventCnt++;
    return 0;
}

/**
 * \ingroup scheduler
 * \brief a lockfree-skiplist event scheduler
 *
 * This class implements the an event scheduler using an lockfree-skiplist
 * data structure.
 */
class LockFreeScheduler
{
public:
  /** Constructor. */
  LockFreeScheduler ();
  /** Destructor. */
  virtual ~LockFreeScheduler ();

  // Inherited
  int Insert (const Scheduler::Event &ev);
  bool IsEmpty (void);
  int PeekNextSlice (shared_ptr<SliceEvents> &sliceEvents);
  uint64_t getEventCount()const {return _eventCnt.load();}
  void gc();
private:

  Time _sliceSize;
  std::atomic<uint64_t> _eventCnt;
  int64x64_t _curSliceId;
  sl_map_gc<int64x64_t, std::shared_ptr<SliceEvents>> _eventTree;

  inline int64x64_t calcSlice(const Time& time)const;
};

inline int64x64_t LockFreeScheduler::calcSlice(const Time& time)const
{
    return time / _sliceSize;
}



} // namespace ns3

#endif /* LOCKFREE_SKIPLIST_SCHEDULER_H */
