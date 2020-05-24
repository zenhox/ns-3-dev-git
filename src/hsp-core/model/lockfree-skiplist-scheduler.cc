#include "lockfree-skiplist-scheduler.h"


namespace ns3 {


LockFreeScheduler::LockFreeScheduler(){
    //5043359
    //_sliceSize = NanoSeconds(1000100);
    _sliceSize = NanoSeconds(150);
    _eventCnt.store(0);
    _curSliceId = 0;
    _events.insert(std::make_pair(0, std::make_shared<EventsMap>()));
}

LockFreeScheduler::~LockFreeScheduler(){

}

void LockFreeScheduler::setSliceSize(uint64_t time)
{
	_sliceSize = NanoSeconds(time);
}

Time LockFreeScheduler::getSliceSize()
{
	return _sliceSize;
}


bool LockFreeScheduler::IsEmpty (void){
    auto itr = _events.find(_curSliceId);
    itr++;
    return itr.isNull();
}

void LockFreeScheduler::gc(int count){
    count--;
    count--;
    auto itr = _events.begin();
    while(count--)
        itr = _events.erase(itr);
}

}

