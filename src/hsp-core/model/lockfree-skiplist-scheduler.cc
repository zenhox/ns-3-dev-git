#include "lockfree-skiplist-scheduler.h"


namespace ns3 {


LockFreeScheduler::LockFreeScheduler(){
	//5043359
    _sliceSize = NanoSeconds(5000000);
    _eventCnt.store(0);
    _curSliceId = 0;
    _events.insert(std::make_pair(0, std::make_shared<EventsMap>()));
}

LockFreeScheduler::~LockFreeScheduler(){

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

