#include "lockfree-skiplist-scheduler.h"


namespace ns3 {

// NS_OBJECT_ENSURE_REGISTERED (LockFreeScheduler);



SliceEvents::SliceEvents(int64x64_t id)
    : _sliceId(id)
{ 

    _eventCnt.store(0);
}

SliceEvents::~SliceEvents(){

}

uint64_t SliceEvents::getEventCount()const{

    return _eventCnt.load();
}

int64x64_t SliceEvents::getSliceId()const{

    return _sliceId;
}

LockFreeScheduler::LockFreeScheduler(){

    _sliceSize = NanoSeconds(1);
    _eventCnt.store(0);
    _curSliceId = 0;
    _eventTree.insert(std::make_pair(0, std::make_shared<SliceEvents>(0)));
}

LockFreeScheduler::~LockFreeScheduler(){

}

// Inherited
int LockFreeScheduler::Insert (const Scheduler::Event &ev){
    Time evTime = Time(ev.key.m_ts);  //猜测
    // cout << "插入了一个事件, time="<<evTime.GetTimeStep()<<endl;
    int64x64_t slice_id = calcSlice(evTime);
    shared_ptr<SliceEvents> sevents = std::make_shared <SliceEvents>(slice_id);
    auto re = _eventTree.insert(std::make_pair(slice_id,sevents));   
    if(slice_id != 0 && slice_id <= _curSliceId)
    {
        // cout<<"发现了片内插入"<<_curSliceId<<endl;
        // 应该是立即调用
        auto itr = _eventTree.find(slice_id);
        (itr->second)->insertEvent(ev);
        _eventCnt++;
        return 0;
    }
    auto itr = _eventTree.find(slice_id);
    (itr->second)->insertEvent(ev);
    _eventCnt++;
    return 0;
}

 bool LockFreeScheduler::IsEmpty (void){

    auto itr = _eventTree.find(_curSliceId);
    itr++;
    return itr.isNull();
}

int LockFreeScheduler::PeekNextSlice (shared_ptr<SliceEvents> &sliceEvents){

    
    static bool isBegin = true;
    auto itr = _eventTree.find(_curSliceId);
    if(_curSliceId == 0)
    {      
        if( isBegin && (itr->second)->getEventCount() != 0)
        {
            sliceEvents = itr->second;
            isBegin = false;
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
    sliceEvents = itr->second;
    return 0;
}

void LockFreeScheduler::gc(){
    int count = 9988;
    auto itr = _eventTree.begin();
    while(count--)
        itr = _eventTree.erase(itr);
}


}

