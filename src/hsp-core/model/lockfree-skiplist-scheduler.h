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

using EventsMap = sl_map_gc<Scheduler::EventKey, EventImpl*>;

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

  inline int PeekNext (shared_ptr<EventsMap>&);
  
  // 没有开始输入-1
  inline double ReadNext(double currTs);  // 读取下一个时间片

  // Inherited
  inline int Insert (const Scheduler::Event &ev);
  inline void Remove (const Scheduler::Event &ev);

  inline bool IsEmpty (void);

  uint64_t getEventCount()const {return _eventCnt.load();}
  double getCurrentSliceId()const{return _curSliceId;}
  inline double calcSlice(const Time& time)const;
  
  void gc();
private:
  Time                     _sliceSize;
  std::atomic<uint64_t>    _eventCnt;
  double                   _curSliceId;
  sl_map_gc<double,  shared_ptr<EventsMap> > _events;
};

inline double LockFreeScheduler::calcSlice(const Time& time)const
{
    return (time / _sliceSize).GetDouble();
}

inline int LockFreeScheduler::PeekNext(shared_ptr<EventsMap> &events){
    static bool isBegin = true;
    auto itr = _events.find(_curSliceId);
    if(_curSliceId == 0)
    {      
        if( isBegin && (itr->second)->size() != 0)
        {
            isBegin = false;
            events = itr->second;

            return 0;
        }else if(isBegin && (itr->second)->size() == 0){
            isBegin = false;
            itr++;
            if(itr.isNull())
              return -1;
            events = itr->second;
            return 0;
        }
    }
    itr++;
    if(itr.isNull())
    {
      _curSliceId = 0;
      return -1;
    }
    _curSliceId = itr->first;
    events = itr->second;
    return 0;
}


inline double LockFreeScheduler::ReadNext(double currTs){
    if( currTs == -1){
      // 还没有开始, 返回第一个
      auto itr = _events.begin();
      if( itr.isNull())
        return -1;
      return itr->first;
    }
    auto itr = _events.find(currTs);
    itr++;
    if(itr.isNull())
    {
        return -1;
    }   
    return itr->first;     
}


inline int LockFreeScheduler::Insert (const Scheduler::Event &ev){
  Time evTime = Time(ev.key.m_ts);  
  double slice_id = calcSlice(evTime);
  shared_ptr<EventsMap> events = std::make_shared <EventsMap>();
  auto re = _events.insert(std::make_pair(slice_id,events)); 
  if(slice_id < _curSliceId)
  {
    return 0;
  }  
  auto itr = _events.find(slice_id);
  (itr->second)->insert(std::make_pair(ev.key,ev.impl));
  _eventCnt++;
  return 0;
}

inline void LockFreeScheduler::Remove (const Scheduler::Event &ev)
{
  Time evTime = Time(ev.key.m_ts); 
  double slice_id = calcSlice(evTime);
  auto itr = _events.find(slice_id);
  if(itr.isNull())
      return;
  (itr->second)->erase(ev.key);
  return;
}


} // namespace ns3

#endif /* LOCKFREE_SKIPLIST_SCHEDULER_H */
