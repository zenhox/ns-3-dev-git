#ifndef _MPI_HELPER_H_
#define _MPI_HELPER_H_
#include <mpi.h>
#include <atomic>
#include "ns3/scheduler.h"
#include "ns3/simulator.h"
#include "lockfree-skiplist-scheduler.h"
#include <iostream>
using namespace std;


#define MAX_CONTEXT_NUM 2


namespace ns3 {


struct MPI_GLB_DATA{
    std::atomic<double> *g_currTimeSlice;          //可以执行哪一个时间片
    std::atomic<double> *g_nextTimeSlice;          // 各个MP去更新，选最小的
    std::atomic<int> * g_status[MAX_CONTEXT_NUM];   // -1 不需要执行  0 执行中 1 执行完毕

    std::atomic<uint32_t> *uid;  
    std::atomic<uint32_t> *sub_uid;  
    std::atomic<uint64_t> *eventCount;
    std::atomic<int64_t>  *minDelay;
    LockFreeScheduler *schedulers[MAX_CONTEXT_NUM];
};

class MPI_Helper{
public:
    static void Enable(){
        // Initialize the MPI environment
        MPI_Init(NULL, NULL);
        MPI_Barrier(MPI_COMM_WORLD);

        MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0,
                MPI_INFO_NULL, &m_comm);

        MPI_Comm_size(m_comm, &m_size);
        // Get the rank of the process
        MPI_Comm_rank(m_comm, &m_rank);

        m_winSize = sizeof(MPI_GLB_DATA);

        if(m_rank == 0)
        {
            MPI_Win_allocate_shared(1, sizeof(MPI_GLB_DATA), MPI_INFO_NULL, m_comm, &m_base_ptr, &m_win);
            MPI_GLB_DATA*  g_data_ptr = (MPI_GLB_DATA*)m_base_ptr;

            (g_data_ptr->g_currTimeSlice) = new std::atomic<double>(0);
            (g_data_ptr->g_nextTimeSlice) = new std::atomic<double>(0);
            (g_data_ptr->uid) = new std::atomic<uint32_t>(4);
            (g_data_ptr->sub_uid) = new std::atomic<uint32_t>(1);
            (g_data_ptr->eventCount) = new std::atomic<uint64_t>(0);
            (g_data_ptr->minDelay) = new std::atomic<int64_t>(999999);
            for(int i =0; i< MAX_CONTEXT_NUM; ++i){
                ((g_data_ptr->g_status)[i]) = new std::atomic<int>(-1);
            }  
            for(int i =0; i< MAX_CONTEXT_NUM; ++i){
                ((g_data_ptr->schedulers)[i]) = new LockFreeScheduler();
            }  
        }
        else{
            int disp_unit; //有几个单元
            MPI_Win_allocate_shared(0, sizeof(MPI_GLB_DATA), MPI_INFO_NULL, m_comm, &m_base_ptr, &m_win);
            MPI_Win_shared_query(m_win, 0, &m_winSize, &disp_unit, &m_base_ptr);
        }

    }

    static void Free(){
        MPI_GLB_DATA*  g_data_ptr = (MPI_GLB_DATA*)m_base_ptr;
        delete  g_data_ptr->g_currTimeSlice;
        delete g_data_ptr->g_nextTimeSlice;
        delete g_data_ptr->uid;
        delete g_data_ptr->sub_uid;
        delete g_data_ptr->eventCount;
        delete g_data_ptr->minDelay;
        MPI_Win_detach(m_win, m_base_ptr->g_currTimeSlice); 
        for(int i =0; i< MAX_CONTEXT_NUM; ++i){
           delete  ((g_data_ptr->g_status)[i]);
        }  
        for(int i =0; i< MAX_CONTEXT_NUM; ++i){
            delete ((g_data_ptr->schedulers)[i]);
        }
        //  
    }

    static void Disable(){
        int flag = 0;
        MPI_Initialized (&flag);
        if (flag)
        {
            Free();
            // free(m_base_ptr);
            // MPI_Win_detach(m_win, m_base_ptr);
            // MPI_Win_free(&m_win);
            MPI_Win_free(&m_win);
            MPI_Finalize ();
        }else{
            cout << "Cannot disable MPI environment without Initializing it first" << endl;
        }
    }

    static int getSystemId(){
        return m_rank;
    }

    static int getSystemNum(){
        return m_size;
    }

    static void* getBasePtr(){
        // return m_base_ptr;
        return nullptr;
    }

private:
    static int m_size;
    static int m_rank;
    static MPI_Comm m_comm;
    static MPI_Win m_win;
    static MPI_Aint m_winSize;
    static MPI_GLB_DATA* m_base_ptr;
};

int MPI_Helper::m_size = 0;
int MPI_Helper::m_rank = 0;

MPI_Comm MPI_Helper::m_comm;
MPI_Win MPI_Helper::m_win;
MPI_Aint MPI_Helper::m_winSize;
MPI_GLB_DATA* MPI_Helper::m_base_ptr;
}




#endif



