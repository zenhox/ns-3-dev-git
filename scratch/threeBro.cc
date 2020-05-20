#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mpi-interface.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-nix-vector-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"

#ifdef NS3_MPI
#include <mpi.h>
#endif

#include <chrono> 
using namespace std;
using namespace chrono;

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleThreeBro");

int
main (int argc, char *argv[])
{
#ifdef NS3_MPI

  bool nix = true;
  bool logging = true;

  // Parse command line
  CommandLine cmd;
  cmd.AddValue ("nix", "Enable the use of nix-vector or global routing", nix);
  cmd.AddValue ("logging", "Enable logging", logging);
  cmd.Parse (argc, argv);

  GlobalValue::Bind ("SimulatorImplementationType",
                    StringValue ("ns3::HspSimualtorImpl"));

  // Enable parallel simulator with the command line arguments
  MpiInterface::Enable (&argc, &argv);

  if(logging)
  {
      LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
  }

  uint32_t systemId = MpiInterface::GetSystemId ();
  //uint32_t systemCount = MpiInterface::GetSize ();

  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (512));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("1Mbps"));
  Config::SetDefault ("ns3::OnOffApplication::MaxBytes", UintegerValue (51200000));

  NodeContainer serverNodes;
  NodeContainer clientNodes;

  // n1(c) n2(s)  n3(c)
  // 1     2      1

  Ptr<Node> n1 = CreateObject<Node> (1);
  Ptr<Node> n2 = CreateObject<Node> (2);
  Ptr<Node> n3 = CreateObject<Node> (1);

  serverNodes.Add(n2);
  clientNodes.Add(n1);
  clientNodes.Add(n3);

  PointToPointHelper link;
  link.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  link.SetChannelAttribute ("Delay", StringValue ("5ms"));

  // Add links for left side leaf nodes to left router
  NetDeviceContainer device_1 = link.Install(n1,n2);
  NetDeviceContainer device_r = link.Install(n2,n3);

  InternetStackHelper stack;
  if (nix)
    {
      Ipv4NixVectorHelper nixRouting;
      stack.SetRoutingHelper (nixRouting); // has effect on the next Install ()
    }

  stack.InstallAll ();


  Ipv4AddressHelper addr_l;
  addr_l.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4AddressHelper addr_r;
  addr_r.SetBase ("10.2.1.0", "255.255.255.0");

  Ipv4InterfaceContainer inf_l = addr_l.Assign(device_1);
  Ipv4InterfaceContainer inf_r = addr_r.Assign(device_r);

  if (!nix)
    {
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    }

  // Create a packet sink on the right leafs to receive packets from left leafs
  uint16_t port = 50000;

  if (systemId == 2)
    {
      Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
      PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
      ApplicationContainer sinkApp;
      sinkApp.Add (sinkHelper.Install ( serverNodes ));
      sinkApp.Start (Seconds (1.0));
      sinkApp.Stop (Seconds (5));
    }
  if (systemId == 1)
    {
      OnOffHelper clientHelper ("ns3::UdpSocketFactory", Address ());
      clientHelper.SetAttribute
        ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      clientHelper.SetAttribute
        ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

      ApplicationContainer clientApps;
      AddressValue remoteAddress(InetSocketAddress (inf_l.GetAddress (1), port));
      clientHelper.SetAttribute ("Remote", remoteAddress);
      clientApps.Add (clientHelper.Install (clientNodes));
      clientApps.Start (Seconds (1.0));
      clientApps.Stop (Seconds (5));
   }


  Simulator::Stop (Seconds (5));

  auto start = system_clock::now();
  Simulator::Run ();
  auto end   = system_clock::now();
  auto duration = duration_cast<microseconds>(end - start);
  if(systemId == 0)
  {
     cout<<"Done. Cost real time: " << double(duration.count()) * microseconds::period::num / microseconds::period::den  << " seconds."<<endl;
     //cout<<"Total event : "<< ns3::Simulator::GetEventCount() <<endl;
  }
  Simulator::Destroy ();
  // Exit the MPI execution environment
  MpiInterface::Disable ();
  return 0;
#else
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif
}
