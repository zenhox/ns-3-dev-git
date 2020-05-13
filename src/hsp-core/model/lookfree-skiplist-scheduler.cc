#include "lockfree-skiplist-scheduler.h"


namespace ns3 {


LockFreeScheduler::LockFreeScheduler()
    :_sliceSize( NanoSeconds(300))
{
    _eventCnt.store(0);
    _curSliceId = 0;
    shared_ptr<EventsMap> evs = std::make_shared<EventsMap>();
    _events = new sl_map_gc<double,  shared_ptr<EventsMap>>();
    _events->insert(std::make_pair(0, evs));
}


LockFreeScheduler::~LockFreeScheduler(){
    delete _events;
}

bool LockFreeScheduler::IsEmpty (void){
    auto itr = _events->find(_curSliceId);
    itr++;
    return itr.isNull();
}

void LockFreeScheduler::gc(){
    int count = 9988;
    auto itr = _events->begin();
    while(count--)
        itr = _events->erase(itr);
}

}

