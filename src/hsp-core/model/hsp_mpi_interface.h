#ifndef NS3_HSP_MPI_INTERFACE_H
#define NS3_HSP_MPI_INTERFACE_H

#include <stdint.h>
#include <list>
#include <atomic>

#include "ns3/nstime.h"
#include "ns3/buffer.h"

#include "ns3/parallel-communication-interface.h"

#ifdef NS3_MPI
#include <mpi.h>
#else
typedef void* MPI_Request;
#endif

#define SYSTEM_SIZE 5

namespace ns3 {

/**
 * maximum MPI message size for easy
 * buffer creation
 */
const uint32_t MAX_MPI_MSG_SIZE2 = 2000;

/**
 * \ingroup mpi
 *
 * \brief Tracks non-blocking sends
 *
 * This class is used to keep track of the asynchronous non-blocking
 * sends that have been posted.
 */
class HspSentBuffer
{
public:
  HspSentBuffer ();
  ~HspSentBuffer ();

  /**
   * \return pointer to sent buffer
   */
  uint8_t* GetBuffer ();
  /**
   * \param buffer pointer to sent buffer
   */
  void SetBuffer (uint8_t* buffer);
  /**
   * \return MPI request
   */
  MPI_Request* GetRequest ();

private:
  uint8_t* m_buffer;
  MPI_Request m_request;
};

class Packet;

struct HspCtlWin{
    std::atomic<double> states[SYSTEM_SIZE];
    std::atomic<double> nextSt[SYSTEM_SIZE];
    std::atomic<double> currSlice;
};

/**
 * \ingroup mpi
 *
 * \brief Interface between ns-3 and MPI
 *
 * Implements the interface used by the singleton parallel controller
 * to interface between NS3 and the communications layer being
 * used for inter-task packet transfers.
 */
class HspMpiInterface : public ParallelCommunicationInterface, Object
{
public:
  static TypeId GetTypeId (void);

  /**
   * Delete all buffers
   */
  virtual void Destroy ();
  /**
   * \return MPI rank
   */
  virtual uint32_t GetSystemId ();
  /**
   * \return MPI size (number of systems)
   */
  virtual uint32_t GetSize ();
  /**
   * \return true if using MPI
   */
  virtual bool IsEnabled ();
  /**
   * \param pargc number of command line arguments
   * \param pargv command line arguments
   *
   * Sets up MPI interface
   */
  virtual void Enable (int* pargc, char*** pargv);
  /**
   * Terminates the MPI environment by calling MPI_Finalize
   * This function must be called after Destroy ()
   * It also resets m_initialized, m_enabled
   */
  virtual void Disable ();
  /**
   * \param p packet to send
   * \param rxTime received time at destination node
   * \param node destination node
   * \param dev destination device
   *
   * Serialize and send a packet to the specified node and net device
   */
  virtual void SendPacket (Ptr<Packet> p, const Time &rxTime, uint32_t node, uint32_t dev);
  /**
   * Check for received messages complete
   */
  static void ReceiveMessages (bool blocking=false);
  /**
   * Check for completed sends
   */
  static void TestSendComplete ();
  /**
   * \return received count in packets
   */
  static uint32_t GetRxCount ();
  /**
   * \return transmitted count in packets
   */
  static uint32_t GetTxCount ();

  /**
   * \return global shared data of ts.
   */
  static double GetCurrTs();
  static double GetMinNextTs();
  /**
   * set global shared data of ts.
   */
  static void SetCurrTs(double ts);
  static void SetNextTs(unsigned sid, double ts);
  /**
   * set/get global shared data of state
   */
  static void SetStatus(unsigned sid, double status);
  static double  GetStatus(unsigned sid);
   

private:
  static uint32_t m_sid;
  static uint32_t m_size;

  // Total packets received
  static uint32_t m_rxCount;

  // Total packets sent
  static uint32_t m_txCount;
  static bool     m_initialized;
  static bool     m_enabled;

  // Pending non-blocking receives
  static MPI_Request* m_requests;

  // Data buffers for non-blocking reads
  static char**   m_pRxBuffers;

  // List of pending non-blocking sends
  static std::list<HspSentBuffer> m_pendingTx;

  // Pointer to global status data.
  static void* m_glbctl_ptr;
  static MPI_Win m_win;
  static MPI_Aint m_winSize;
};

} // namespace ns3

#endif /* NS3_GRANTED_TIME_WINDOW_MPI_INTERFACE_H */
