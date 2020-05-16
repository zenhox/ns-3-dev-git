#include <iostream>
#include <iomanip>
#include <list>

#include "hsp_mpi_interface.h"
#include "ns3/mpi-receiver.h"
#include "ns3/mpi-interface.h"

#include "ns3/node.h"
#include "ns3/node-list.h"
#include "ns3/net-device.h"
#include "ns3/simulator.h"
#include "ns3/simulator-impl.h"
#include "ns3/nstime.h"
#include "ns3/log.h"



namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("HspMpiInterface");

HspSentBuffer::HspSentBuffer ()
{
  m_buffer = 0;
  m_request = 0;
}

HspSentBuffer::~HspSentBuffer ()
{
  delete [] m_buffer;
}

uint8_t*
HspSentBuffer::GetBuffer ()
{
  return m_buffer;
}

void
HspSentBuffer::SetBuffer (uint8_t* buffer)
{
  m_buffer = buffer;
}

#ifdef NS3_MPI
MPI_Request*
HspSentBuffer::GetRequest ()
{
  return &m_request;
}
#endif

uint32_t                 HspMpiInterface::m_sid = 0;
uint32_t                 HspMpiInterface::m_size = 1;
bool                     HspMpiInterface::m_initialized = false;
bool                     HspMpiInterface::m_enabled = false;
uint32_t                 HspMpiInterface::m_rxCount = 0;
uint32_t                 HspMpiInterface::m_txCount = 0;
std::list<HspSentBuffer> HspMpiInterface::m_pendingTx;



#ifdef NS3_MPI
MPI_Request*             HspMpiInterface::m_requests;
char**                   HspMpiInterface::m_pRxBuffers;

void*                    HspMpiInterface::m_glbctl_ptr = nullptr;
MPI_Win                  HspMpiInterface::m_win;
MPI_Aint                 HspMpiInterface::m_winSize = sizeof(HspCtlWin);
#endif

TypeId 
HspMpiInterface::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HspMpiInterface")
    .SetParent<Object> ()
    .SetGroupName ("Hsp-Core")
  ;
  return tid;
}

void
HspMpiInterface::Destroy ()
{
  NS_LOG_FUNCTION (this);

#ifdef NS3_MPI
  for (uint32_t i = 0; i < GetSize (); ++i)
    {
      delete [] m_pRxBuffers[i];
    }
  delete [] m_pRxBuffers;
  delete [] m_requests;

  m_pendingTx.clear ();
#endif
}

uint32_t
HspMpiInterface::GetRxCount ()
{
  return m_rxCount;
}

uint32_t
HspMpiInterface::GetTxCount ()
{
  return m_txCount;
}

uint32_t
HspMpiInterface::GetSystemId ()
{
  if (!m_initialized)
    {
      Simulator::GetImplementation ();
      m_initialized = true;
    }
  return m_sid;
}

uint32_t
HspMpiInterface::GetSize ()
{
  if (!m_initialized)
    {
      Simulator::GetImplementation ();
      m_initialized = true;
    }
  return m_size;
}

bool
HspMpiInterface::IsEnabled ()
{
  if (!m_initialized)
    {
      Simulator::GetImplementation ();
      m_initialized = true;
    }
  return m_enabled;
}

void
HspMpiInterface::Enable (int* pargc, char*** pargv)
{
  NS_LOG_FUNCTION (this << pargc << pargv); 

#ifdef NS3_MPI
  // Initialize the MPI interface
  MPI_Init (pargc, pargv);
  MPI_Barrier (MPI_COMM_WORLD);
  MPI_Comm_rank (MPI_COMM_WORLD, reinterpret_cast <int *> (&m_sid));
  MPI_Comm_size (MPI_COMM_WORLD, reinterpret_cast <int *> (&m_size));
  m_enabled = true;
  m_initialized = true;
  // Post a non-blocking receive for all peers
  m_pRxBuffers = new char*[m_size];
  m_requests = new MPI_Request[m_size];
  for (uint32_t i = 0; i < GetSize (); ++i)
    {
      m_pRxBuffers[i] = new char[MAX_MPI_MSG_SIZE2];
      MPI_Irecv (m_pRxBuffers[i], MAX_MPI_MSG_SIZE2, MPI_CHAR, MPI_ANY_SOURCE, 0,
                 MPI_COMM_WORLD, &m_requests[i]);
    }
  // Alloc shared memory
  if(m_sid == 0)
    {
        MPI_Win_allocate_shared(1, sizeof(HspCtlWin), MPI_INFO_NULL, MPI_COMM_WORLD, &m_glbctl_ptr, &m_win);
        /** Init the global data.*/
        HspCtlWin* _ptr= (HspCtlWin*) m_glbctl_ptr;
        (_ptr->currSlice).store(0);
        // (_ptr->nextSlice).store(0);
        for(unsigned i = 1; i < m_size; ++i){
          (_ptr->states)[i].store(-1); 
        }
        for(unsigned i = 1; i < m_size; ++i){
          (_ptr->nextSt)[i].store(0); 
        }
    }
    else{
        int disp_unit; //有几个单元
        MPI_Win_allocate_shared(0, sizeof(HspCtlWin), MPI_INFO_NULL, MPI_COMM_WORLD, &m_glbctl_ptr, &m_win);
        MPI_Win_shared_query(m_win, 0, &m_winSize, &disp_unit, &m_glbctl_ptr);
    }
    MPI_Barrier (MPI_COMM_WORLD);
#else
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif
}

void
HspMpiInterface::SendPacket (Ptr<Packet> p, const Time& rxTime, uint32_t node, uint32_t dev)
{
  NS_LOG_FUNCTION (this << p << rxTime.GetTimeStep () << node << dev);
  // std::cout << "调用了SendPacket" << std::endl;
#ifdef NS3_MPI
  HspSentBuffer sendBuf;
  m_pendingTx.push_back (sendBuf);
  std::list<HspSentBuffer>::reverse_iterator i = m_pendingTx.rbegin (); // Points to the last element

  uint32_t serializedSize = p->GetSerializedSize ();
  uint8_t* buffer =  new uint8_t[serializedSize + 16];
  i->SetBuffer (buffer);
  // Add the time, dest node and dest device
  uint64_t t = rxTime.GetInteger ();
  uint64_t* pTime = reinterpret_cast <uint64_t *> (buffer);
  *pTime++ = t;
  uint32_t* pData = reinterpret_cast<uint32_t *> (pTime);
  *pData++ = node;
  *pData++ = dev;
  // Serialize the packet
  p->Serialize (reinterpret_cast<uint8_t *> (pData), serializedSize);

  // Find the system id for the destination node
  Ptr<Node> destNode = NodeList::GetNode (node);
  uint32_t nodeSysId = destNode->GetSystemId ();

  MPI_Isend (reinterpret_cast<void *> (i->GetBuffer ()), serializedSize + 16, MPI_CHAR, nodeSysId,
             0, MPI_COMM_WORLD, (i->GetRequest ()));
  m_txCount++;
#else
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif
}

void
HspMpiInterface::ReceiveMessages (bool blocking)
{ 
  NS_LOG_FUNCTION_NOARGS ();
  // std::cout << "调用了接收消息" << std::endl;
#ifdef NS3_MPI
    bool stop = false;
  // Poll the non-block reads to see if data arrived
  while (!stop)
    {
      int flag = 0;
      int index = 0;
      MPI_Status status;

      if(!blocking)
      {
          MPI_Testany (MpiInterface::GetSize (), m_requests, &index, &flag, &status);
      }
      else{
          MPI_Waitany (MpiInterface::GetSize (), m_requests, &index, &status);
          flag = 1;
          stop = true;
      }
     
      if (!flag)
        {
          // std::cout<<"没有消息" <<std::endl;
          break;        // No more messages
        }
        // std::cout<<"有消息" <<std::endl;
      int count;
      MPI_Get_count (&status, MPI_CHAR, &count);
      m_rxCount++; // Count this receive

      // Get the meta data first
      uint64_t* pTime = reinterpret_cast<uint64_t *> (m_pRxBuffers[index]);
      uint64_t time = *pTime++;
      uint32_t* pData = reinterpret_cast<uint32_t *> (pTime);
      uint32_t node = *pData++;
      uint32_t dev  = *pData++;

      Time rxTime (time);

      count -= sizeof (time) + sizeof (node) + sizeof (dev);

      Ptr<Packet> p = Create<Packet> (reinterpret_cast<uint8_t *> (pData), count, true);

      // Find the correct node/device to schedule receive event
      Ptr<Node> pNode = NodeList::GetNode (node);
      Ptr<MpiReceiver> pMpiRec = 0;
      uint32_t nDevices = pNode->GetNDevices ();
      for (uint32_t i = 0; i < nDevices; ++i)
        {
          Ptr<NetDevice> pThisDev = pNode->GetDevice (i);
          if (pThisDev->GetIfIndex () == dev)
            {
              pMpiRec = pThisDev->GetObject<MpiReceiver> ();
              break;
            }
        }

      NS_ASSERT (pNode && pMpiRec);

      // Schedule the rx event
      Simulator::ScheduleWithContext (pNode->GetId (), rxTime - Simulator::Now (),
                                      &MpiReceiver::Receive, pMpiRec, p);
      // std::cout << "收到了消息" << std::endl;

      // Re-queue the next read
      MPI_Irecv (m_pRxBuffers[index], MAX_MPI_MSG_SIZE2, MPI_CHAR, MPI_ANY_SOURCE, 0,
                 MPI_COMM_WORLD, &m_requests[index]);
    }
#else
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif
}

void
HspMpiInterface::TestSendComplete ()
{
  NS_LOG_FUNCTION_NOARGS ();

#ifdef NS3_MPI
  std::list<HspSentBuffer>::iterator i = m_pendingTx.begin ();
  while (i != m_pendingTx.end ())
    {
      MPI_Status status;
      int flag = 0;
      MPI_Test (i->GetRequest (), &flag, &status);
      std::list<HspSentBuffer>::iterator current = i; // Save current for erasing
      i++;                                    // Advance to next
      if (flag)
        { // This message is complete
          // std::cout << "发送成功了呀" << std::endl;
          m_pendingTx.erase (current);
        }
    }
#else
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif
}

void
HspMpiInterface::Disable ()
{
  NS_LOG_FUNCTION_NOARGS ();

#ifdef NS3_MPI
  int flag = 0;
  MPI_Initialized (&flag);
  if (flag)
    {
      MPI_Win_free(&m_win);
      MPI_Finalize ();
      m_enabled = false;
      m_initialized = false;
    }
  else
    {
      NS_FATAL_ERROR ("Cannot disable MPI environment without Initializing it first");
    }
#else
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif
}


double 
HspMpiInterface::GetCurrTs()
{
  NS_LOG_FUNCTION_NOARGS ();

  NS_ASSERT( m_glbctl_ptr != nullptr);
  HspCtlWin* _ptr= (HspCtlWin*) m_glbctl_ptr;
  return (_ptr->currSlice).load();
}

double 
HspMpiInterface::GetMinNextTs()
{
  NS_LOG_FUNCTION_NOARGS ();
  // static int count = 10;
  NS_ASSERT( m_glbctl_ptr != nullptr);
  HspCtlWin* _ptr= (HspCtlWin*) m_glbctl_ptr;
  double min = -1;
  for(unsigned i = 1; i < m_size; ++i){
    double ts = (_ptr->nextSt)[i].load();  
    // if(count!=0){
    //   count--;
    // std::cout << ts << " ";
    // } 
    if( ts != -1 && (min == -1 || ts < min) )
        min = ts;
  }
  // std::cout << std::endl;
  return min;
}


void 
HspMpiInterface::SetCurrTs(double ts)
{
  NS_LOG_FUNCTION_NOARGS ();

  NS_ASSERT( m_glbctl_ptr != nullptr);
  HspCtlWin* _ptr= (HspCtlWin*) m_glbctl_ptr;
  (_ptr->currSlice).store(ts);
}


void 
HspMpiInterface::SetNextTs(unsigned sid, double ts)
{
  NS_LOG_FUNCTION_NOARGS ();

  NS_ASSERT( m_glbctl_ptr != nullptr);
  HspCtlWin* _ptr= (HspCtlWin*) m_glbctl_ptr;
  // if(m_sid == 1)
  // {
  //   std::cout << "设置了 nextTs = " << ts << std::endl;
  // }
  (_ptr->nextSt)[sid].store(ts);
}


void 
HspMpiInterface::SetStatus(unsigned sid, double status)
{
  NS_LOG_FUNCTION_NOARGS ();

  NS_ASSERT( m_glbctl_ptr != nullptr);

  HspCtlWin* _ptr= (HspCtlWin*) m_glbctl_ptr;
  (_ptr->states)[sid].store(status); 
}

double  
HspMpiInterface::GetStatus(unsigned sid)
{
  NS_LOG_FUNCTION_NOARGS ();

  NS_ASSERT( m_glbctl_ptr != nullptr);

  HspCtlWin* _ptr= (HspCtlWin*) m_glbctl_ptr;
  return (_ptr->states)[sid].load(); 
}


} // namespace ns3
