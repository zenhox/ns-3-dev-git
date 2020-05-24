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
#include <iostream>
#include <chrono> 
#include <map>
using namespace std;
using namespace chrono;

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleThreeBro");

struct GridArgs{
	unsigned xSize;
	unsigned ySize;
	double startTimeBase;
	double endTimeBase;
};

void mode1func(GridArgs args);
void mode2func(GridArgs args);
void mode3func(GridArgs args);

int
main (int argc, char *argv[])
{
#ifdef NS3_MPI

  //bool nix = true;

  bool logging = false;

  int  mode = 1;  /**1 : normal, 2 : nullmessage 3 : hsp*/
  GridArgs args;
  args.xSize = 3;
  args.ySize = 50;
  args.startTimeBase = 1;
  args.endTimeBase = 3;


  // Parse command line
  CommandLine cmd;
  //cmd.AddValue ("nix", "Enable the use of nix-vector or global routing", nix);
  cmd.AddValue ("logging", "Enable logging", logging);
  cmd.AddValue ("mode", "Select mode", mode);
  cmd.Parse (argc, argv);

  if(logging)
  {
      LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
  }

  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (512));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("100Mbps"));
  Config::SetDefault ("ns3::OnOffApplication::MaxBytes", UintegerValue (40960));

  if(mode == 1)
  {
      printf("Start one process one thread mode...\n");
      mode1func(args);
  }
  else if(mode == 2)
  {
      GlobalValue::Bind ("SimulatorImplementationType",
			  StringValue ("ns3::NullMessageSimulatorImpl"));

      // Enable parallel simulator with the command line arguments
      MpiInterface::Enable (&argc, &argv);
      uint32_t systemId = MpiInterface::GetSystemId ();
      uint32_t systemCount = MpiInterface::GetSize ();
      if(systemId == 0)
      {
          printf("Start %u system to accelarate simulation...\n", systemCount);
          printf("Using null message core.\n");
      }
      mode2func(args);
  }
  else if (mode == 3)
  {
      GlobalValue::Bind ("SimulatorImplementationType",
                        StringValue ("ns3::HspSimualtorImpl"));
      // Enable parallel simulator with the command line arguments
      MpiInterface::Enable (&argc, &argv);
      uint32_t systemId = MpiInterface::GetSystemId ();
      uint32_t systemCount = MpiInterface::GetSize ();
      if(systemCount < 2)
      {
          printf("System count need at least 2!!\n");
          return 0;
      }
      if(systemId == 0)
      {
          printf("Start %u system to accelarate simulation...\n", systemCount);
          printf("Using hsp time slice core.\n");
      }
      mode3func(args);
  }
  else if (mode == 4)
  {
      GlobalValue::Bind ("SimulatorImplementationType",
		StringValue ("ns3::DistributedSimulatorImpl"));
      // Enable parallel simulator with the command line arguments
      MpiInterface::Enable (&argc, &argv);
      uint32_t systemId = MpiInterface::GetSystemId ();
      uint32_t systemCount = MpiInterface::GetSize ();
      if(systemId == 0)
      {
          printf("Start %u system to accelarate simulation...\n", systemCount);
          printf("Using distribute core.\n");
      }
      mode2func(args);
  }
  return 0;
#else
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif
}


void mode3func(GridArgs args)
{

  map<uint32_t, uint32_t> sidMap;
  uint32_t systemId = MpiInterface::GetSystemId ();
  uint32_t systemCount = MpiInterface::GetSize ();

  //Create this nodes.
  NodeContainer gridNodes;
  uint32_t system_alloc = 1;
  for(unsigned i = 0; i < args.xSize; ++i)
  {
      for(unsigned j = 0; j < args.ySize; ++j)
      {
    	  uint32_t sid = (system_alloc % (systemCount-1)) + 1;
    	  system_alloc++;
    	  Ptr<Node> node = CreateObject<Node>(sid);
    	  gridNodes.Add(node);
    	  sidMap.insert(make_pair( node->GetId(), sid));
      }
  }
  //cout << "sidMap size = " << sidMap.size() << endl;
  // Add links for left side leaf nodes to left router
  vector<NetDeviceContainer> colDevices( (args.xSize-1) * args.ySize );
  vector<NetDeviceContainer> rowDevices( args.xSize * (args.ySize - 1));
  vector<Ipv4InterfaceContainer> colInfs((args.xSize-1) * args.ySize);
  vector<Ipv4InterfaceContainer> rowInfs( args.xSize * (args.ySize - 1) ); //= addrs.Assign(devices);
  
  InternetStackHelper stack;
  Ipv4NixVectorHelper nixRouting;
  stack.SetRoutingHelper (nixRouting); // has effect on the next Install ()
  stack.InstallAll ();

  PointToPointHelper link;
  link.SetDeviceAttribute ("DataRate", StringValue ("40Gbps"));
  link.SetChannelAttribute ("Delay", StringValue ("50ns"));
  unsigned pos = 0;
  // xsize == 1
  // ysize == 2
  for(unsigned i = 0; i < args.xSize; ++i)
  {
      for(unsigned j = 1; j < args.ySize; ++j)
      {
          uint32_t index = i * args.xSize + j;
          uint32_t left = i * args.xSize + j - 1;
          Ipv4AddressHelper addrs;
          addrs.SetBase ( ("10." + to_string(i) + "."+to_string(j)+".0").c_str(), "255.255.255.0");
          NetDeviceContainer tempDev =  link.Install(  gridNodes.Get(left), gridNodes.Get(index));
          Ipv4InterfaceContainer tempInf = addrs.Assign( tempDev);
          rowDevices[pos].Add(tempDev.Get(0));
          rowDevices[pos].Add(tempDev.Get(1));
          rowInfs[pos].Add(tempInf.Get(0));
          rowInfs[pos].Add(tempInf.Get(1));
          pos++;
      }
  }
  pos = 0;
  for(unsigned i = 1; i < args.xSize; ++i)
  {
      for(unsigned j = 0; j < args.ySize; ++j)
      {
          uint32_t index = i * args.xSize + j;
          uint32_t top = (i-1) * args.xSize + j;
          Ipv4AddressHelper addrs;
          addrs.SetBase ( ("20." + to_string(i) + "."+to_string(j)+".0").c_str(), "255.255.255.0");
          NetDeviceContainer tempDev =  link.Install(gridNodes.Get(top), gridNodes.Get(index));
          Ipv4InterfaceContainer tempInf = addrs.Assign( tempDev);
          colDevices[pos].Add(tempDev.Get(0));
          colDevices[pos].Add(tempDev.Get(1));
          colInfs[pos].Add(tempInf.Get(0));
          colInfs[pos].Add(tempInf.Get(1));
          pos++;
      }
  }
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp;
  for(unsigned i = 0; i < args.xSize * args.ySize; ++i)
  {
          Ptr<Node> node = gridNodes.Get(i);
          uint32_t node_id = node->GetId();
          if( sidMap[node_id] == systemId )
          {
              sinkApp.Add (sinkHelper.Install ( node  ));
          }
  }
  sinkApp.Start (Seconds (args.startTimeBase));
  sinkApp.Stop (Seconds (args.endTimeBase));
  
  
  //所有这个sid 的安装到其他所有节点的客户端
  OnOffHelper clientHelper ("ns3::UdpSocketFactory", Address ());
  clientHelper.SetAttribute
    ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute
    ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer clientApps;
  for(unsigned i = 0; i < args.xSize * args.ySize; ++i)
  {
          Ptr<Node> node = gridNodes.Get(i);
          uint32_t node_id = node->GetId();
          if( sidMap[node_id] == systemId )
          {
              for(uint32_t j = 0; j < args.xSize * args.ySize; ++j)
              {
                   Ptr<Node> nd = gridNodes.Get(j);
                   //uint32_t nid = nd->GetId();
                   if( i != j )
                   { 
			   if(rowInfs.size() > 0)
			   {
			       uint32_t rowId = j / args.ySize;
			       uint32_t colId = j % args.ySize;
			       if(colId > 0)
			       {
			           uint32_t infIndex = j - rowId - 1;
                                   //安装到node的客户端
                                   AddressValue remoteAddress(InetSocketAddress (rowInfs[infIndex].GetAddress(1), port ));
                                   clientHelper.SetAttribute ("Remote", remoteAddress);
                                   clientApps.Add (clientHelper.Install ( node ));
			       }
			       else
			       {
			           uint32_t infIndex = j - rowId;
                                   //安装到node的客户端
                                   AddressValue remoteAddress(InetSocketAddress (rowInfs[infIndex].GetAddress(0), port ));
                                   clientHelper.SetAttribute ("Remote", remoteAddress);
                                   clientApps.Add (clientHelper.Install ( node ));
			       }
			   }
			   else if(colInfs.size() > 0){
			       uint32_t rowId = j / args.ySize;
			       //uint32_t colId = j % args.ySize;
			       if(rowId < args.xSize - 1)
			       {
			           uint32_t infIndex = j;
                                   //安装到node的客户端
                                   AddressValue remoteAddress(InetSocketAddress (colInfs[infIndex].GetAddress(0), port ));
                                   clientHelper.SetAttribute ("Remote", remoteAddress);
                                   clientApps.Add (clientHelper.Install ( node ));
			       }
			       else
			       {
			           uint32_t infIndex = j - args.xSize;
                                   //安装到node的客户端
                                   AddressValue remoteAddress(InetSocketAddress (colInfs[infIndex].GetAddress(1), port ));
                                   clientHelper.SetAttribute ("Remote", remoteAddress);
                                   clientApps.Add (clientHelper.Install ( node ));
			       }
			   }
                   }
              }
          }
  }
  //cout << "flag " << systemId << endl;
  //Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
  //x->SetAttribute ("Min", DoubleValue (0));
  //x->SetAttribute ("Max", DoubleValue (0.001));
  //double rn = x->GetValue ();
  clientApps.Start (Seconds (args.startTimeBase));
  clientApps.Stop (Seconds (args.endTimeBase));

  Simulator::Stop (Seconds (args.endTimeBase));
  
  auto start = system_clock::now();
  //cout << "开始了吗?"  << systemId << endl;
  Simulator::Run ();
  //cout << "结束了吗?" << systemId <<endl;
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
}

void mode1func(GridArgs args)
{

  //Create this nodes.
  NodeContainer gridNodes;
  for(unsigned i = 0; i < args.xSize; ++i)
  {
      for(unsigned j = 0; j < args.ySize; ++j)
      {
    	  Ptr<Node> node = CreateObject<Node>();
    	  gridNodes.Add(node);
      }
  }

  vector<NetDeviceContainer> colDevices( (args.xSize-1) * args.ySize );
  vector<NetDeviceContainer> rowDevices( args.xSize * (args.ySize - 1));
  vector<Ipv4InterfaceContainer> colInfs((args.xSize-1) * args.ySize);
  vector<Ipv4InterfaceContainer> rowInfs( args.xSize * (args.ySize - 1) ); //= addrs.Assign(devices);
  
  InternetStackHelper stack;
  Ipv4NixVectorHelper nixRouting;
  stack.SetRoutingHelper (nixRouting); // has effect on the next Install ()
  stack.InstallAll ();

  PointToPointHelper link;
  link.SetDeviceAttribute ("DataRate", StringValue ("40Gbps"));
  link.SetChannelAttribute ("Delay", StringValue ("50ns"));
  unsigned pos = 0;
  for(unsigned i = 0; i < args.xSize; ++i)
  {
      for(unsigned j = 1; j < args.ySize; ++j)
      {
          uint32_t index = i * args.xSize + j;
          uint32_t left = i * args.xSize + j - 1;
          Ipv4AddressHelper addrs;
          addrs.SetBase ( ("10." + to_string(i) + "."+to_string(j)+".0").c_str(), "255.255.255.0");
          NetDeviceContainer tempDev =  link.Install(  gridNodes.Get(left), gridNodes.Get(index));
          Ipv4InterfaceContainer tempInf = addrs.Assign( tempDev);
          rowDevices[pos].Add(tempDev.Get(0));
          rowDevices[pos].Add(tempDev.Get(1));
          rowInfs[pos].Add(tempInf.Get(0));
          rowInfs[pos].Add(tempInf.Get(1));
          pos++;
      }
  }
  pos = 0;
  for(unsigned i = 1; i < args.xSize; ++i)
  {
      for(unsigned j = 0; j < args.ySize; ++j)
      {
          uint32_t index = i * args.xSize + j;
          uint32_t top = (i-1) * args.xSize + j;
          Ipv4AddressHelper addrs;
          addrs.SetBase ( ("20." + to_string(i) + "."+to_string(j)+".0").c_str(), "255.255.255.0");
          NetDeviceContainer tempDev =  link.Install(gridNodes.Get(top), gridNodes.Get(index));
          Ipv4InterfaceContainer tempInf = addrs.Assign( tempDev);
          colDevices[pos].Add(tempDev.Get(0));
          colDevices[pos].Add(tempDev.Get(1));
          colInfs[pos].Add(tempInf.Get(0));
          colInfs[pos].Add(tempInf.Get(1));
          pos++;
      }
  }
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp;
  for(unsigned i = 0; i < args.xSize * args.ySize; ++i)
  {
          Ptr<Node> node = gridNodes.Get(i);
          //uint32_t node_id = node->GetId();
          sinkApp.Add (sinkHelper.Install ( node  ));
  }
  sinkApp.Start (Seconds (args.startTimeBase));
  sinkApp.Stop (Seconds (args.endTimeBase));
  
  
  //所有这个sid 的安装到其他所有节点的客户端
  OnOffHelper clientHelper ("ns3::UdpSocketFactory", Address ());
  clientHelper.SetAttribute
    ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute
    ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer clientApps;
  for(unsigned i = 0; i < args.xSize * args.ySize; ++i)
  {
          Ptr<Node> node = gridNodes.Get(i);
          //uint32_t node_id = node->GetId();
          for(uint32_t j = 0; j < args.xSize * args.ySize; ++j)
          {
               Ptr<Node> nd = gridNodes.Get(j);
               //uint32_t nid = nd->GetId();
               if( i != j )
               { 
	    	   if(rowInfs.size() > 0)
	    	   {
	    	       uint32_t rowId = j / args.ySize;
	    	       uint32_t colId = j % args.ySize;
	    	       if(colId > 0)
	    	       {
	    	           uint32_t infIndex = j - rowId - 1;
                               //安装到node的客户端
                               AddressValue remoteAddress(InetSocketAddress (rowInfs[infIndex].GetAddress(1), port ));
                               clientHelper.SetAttribute ("Remote", remoteAddress);
                               clientApps.Add (clientHelper.Install ( node ));
	    	       }
	    	       else
	    	       {
	    	           uint32_t infIndex = j - rowId;
                               //安装到node的客户端
                               AddressValue remoteAddress(InetSocketAddress (rowInfs[infIndex].GetAddress(0), port ));
                               clientHelper.SetAttribute ("Remote", remoteAddress);
                               clientApps.Add (clientHelper.Install ( node ));
	    	       }
	    	   }
	    	   else if(colInfs.size() > 0){
	    	       uint32_t rowId = j / args.ySize;
	    	       //uint32_t colId = j % args.ySize;
	    	       if(rowId < args.xSize - 1)
	    	       {
	    	           uint32_t infIndex = j;
                               //安装到node的客户端
                               AddressValue remoteAddress(InetSocketAddress (colInfs[infIndex].GetAddress(0), port ));
                               clientHelper.SetAttribute ("Remote", remoteAddress);
                               clientApps.Add (clientHelper.Install ( node ));
	    	       }
	    	       else
	    	       {
	    	           uint32_t infIndex = j - args.xSize;
                               //安装到node的客户端
                               AddressValue remoteAddress(InetSocketAddress (colInfs[infIndex].GetAddress(1), port ));
                               clientHelper.SetAttribute ("Remote", remoteAddress);
                               clientApps.Add (clientHelper.Install ( node ));
	    	       }
	    	   }
               }
           }
  }
  //Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
  //x->SetAttribute ("Min", DoubleValue (0));
  //x->SetAttribute ("Max", DoubleValue (0.001));
  //double rn = x->GetValue ();
  clientApps.Start (Seconds (args.startTimeBase));
  clientApps.Stop (Seconds (args.endTimeBase));

  Simulator::Stop (Seconds (args.endTimeBase));
  auto start = system_clock::now();
  Simulator::Run ();
  auto end   = system_clock::now();
  auto duration = duration_cast<microseconds>(end - start);
  cout<<"Done. Cost real time: " << double(duration.count()) * microseconds::period::num / microseconds::period::den  << " seconds."<<endl;
  Simulator::Destroy ();
}

void mode2func(GridArgs args)
{

  map<uint32_t, uint32_t> sidMap;
  uint32_t systemId = MpiInterface::GetSystemId ();
  uint32_t systemCount = MpiInterface::GetSize ();

  //Create this nodes.
  NodeContainer gridNodes;
  uint32_t system_alloc = 0;
  for(unsigned i = 0; i < args.xSize; ++i)
  {
      for(unsigned j = 0; j < args.ySize; ++j)
      {
    	  uint32_t sid = system_alloc % systemCount;
    	  system_alloc++;
    	  Ptr<Node> node = CreateObject<Node>(sid);
    	  gridNodes.Add(node);
    	  sidMap.insert(make_pair( node->GetId(), sid));
      }
  }
  //cout << "sidMap size = " << sidMap.size() << endl;
  // Add links for left side leaf nodes to left router
  vector<NetDeviceContainer> colDevices( (args.xSize-1) * args.ySize );
  vector<NetDeviceContainer> rowDevices( args.xSize * (args.ySize - 1));
  vector<Ipv4InterfaceContainer> colInfs((args.xSize-1) * args.ySize);
  vector<Ipv4InterfaceContainer> rowInfs( args.xSize * (args.ySize - 1) ); //= addrs.Assign(devices);
  
  InternetStackHelper stack;
  Ipv4NixVectorHelper nixRouting;
  stack.SetRoutingHelper (nixRouting); // has effect on the next Install ()
  stack.InstallAll ();

  PointToPointHelper link;
  link.SetDeviceAttribute ("DataRate", StringValue ("40Gbps"));
  link.SetChannelAttribute ("Delay", StringValue ("50ns"));
  unsigned pos = 0;
  // xsize == 1
  // ysize == 2
  for(unsigned i = 0; i < args.xSize; ++i)
  {
      for(unsigned j = 1; j < args.ySize; ++j)
      {
          uint32_t index = i * args.xSize + j;
          uint32_t left = i * args.xSize + j - 1;
          Ipv4AddressHelper addrs;
          addrs.SetBase ( ("10." + to_string(i) + "."+to_string(j)+".0").c_str(), "255.255.255.0");
          NetDeviceContainer tempDev =  link.Install(  gridNodes.Get(left), gridNodes.Get(index));
          Ipv4InterfaceContainer tempInf = addrs.Assign( tempDev);
          rowDevices[pos].Add(tempDev.Get(0));
          rowDevices[pos].Add(tempDev.Get(1));
          rowInfs[pos].Add(tempInf.Get(0));
          rowInfs[pos].Add(tempInf.Get(1));
          pos++;
      }
  }
  pos = 0;
  for(unsigned i = 1; i < args.xSize; ++i)
  {
      for(unsigned j = 0; j < args.ySize; ++j)
      {
          uint32_t index = i * args.xSize + j;
          uint32_t top = (i-1) * args.xSize + j;
          Ipv4AddressHelper addrs;
          addrs.SetBase ( ("20." + to_string(i) + "."+to_string(j)+".0").c_str(), "255.255.255.0");
          NetDeviceContainer tempDev =  link.Install(gridNodes.Get(top), gridNodes.Get(index));
          Ipv4InterfaceContainer tempInf = addrs.Assign( tempDev);
          colDevices[pos].Add(tempDev.Get(0));
          colDevices[pos].Add(tempDev.Get(1));
          colInfs[pos].Add(tempInf.Get(0));
          colInfs[pos].Add(tempInf.Get(1));
          pos++;
      }
  }
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp;
  for(unsigned i = 0; i < args.xSize * args.ySize; ++i)
  {
          Ptr<Node> node = gridNodes.Get(i);
          uint32_t node_id = node->GetId();
          if( sidMap[node_id] == systemId )
          {
              sinkApp.Add (sinkHelper.Install ( node  ));
          }
  }
  sinkApp.Start (Seconds (args.startTimeBase));
  sinkApp.Stop (Seconds (args.endTimeBase));
  
  
  //所有这个sid 的安装到其他所有节点的客户端
  OnOffHelper clientHelper ("ns3::UdpSocketFactory", Address ());
  clientHelper.SetAttribute
    ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute
    ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer clientApps;
  for(unsigned i = 0; i < args.xSize * args.ySize; ++i)
  {
          Ptr<Node> node = gridNodes.Get(i);
          uint32_t node_id = node->GetId();
          if( sidMap[node_id] == systemId )
          {
              for(uint32_t j = 0; j < args.xSize * args.ySize; ++j)
              {
                   Ptr<Node> nd = gridNodes.Get(j);
                   //uint32_t nid = nd->GetId();
                   if( i != j )
                   { 
			   if(rowInfs.size() > 0)
			   {
			       uint32_t rowId = j / args.ySize;
			       uint32_t colId = j % args.ySize;
			       if(colId > 0)
			       {
			           uint32_t infIndex = j - rowId - 1;
                                   //安装到node的客户端
                                   AddressValue remoteAddress(InetSocketAddress (rowInfs[infIndex].GetAddress(1), port ));
                                   clientHelper.SetAttribute ("Remote", remoteAddress);
                                   clientApps.Add (clientHelper.Install ( node ));
			       }
			       else
			       {
			           uint32_t infIndex = j - rowId;
                                   //安装到node的客户端
                                   AddressValue remoteAddress(InetSocketAddress (rowInfs[infIndex].GetAddress(0), port ));
                                   clientHelper.SetAttribute ("Remote", remoteAddress);
                                   clientApps.Add (clientHelper.Install ( node ));
			       }
			   }
			   else if(colInfs.size() > 0){
			       uint32_t rowId = j / args.ySize;
			       //uint32_t colId = j % args.ySize;
			       if(rowId < args.xSize - 1)
			       {
			           uint32_t infIndex = j;
                                   //安装到node的客户端
                                   AddressValue remoteAddress(InetSocketAddress (colInfs[infIndex].GetAddress(0), port ));
                                   clientHelper.SetAttribute ("Remote", remoteAddress);
                                   clientApps.Add (clientHelper.Install ( node ));
			       }
			       else
			       {
			           uint32_t infIndex = j - args.xSize;
                                   //安装到node的客户端
                                   AddressValue remoteAddress(InetSocketAddress (colInfs[infIndex].GetAddress(1), port ));
                                   clientHelper.SetAttribute ("Remote", remoteAddress);
                                   clientApps.Add (clientHelper.Install ( node ));
			       }
			   }
                   }
              }
          }
  }
  ////cout << "flag " << systemId << endl;
  //Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
  //x->SetAttribute ("Min", DoubleValue (0));
  //x->SetAttribute ("Max", DoubleValue (0.001));
  //double rn = x->GetValue ();
  clientApps.Start (Seconds (args.startTimeBase));
  clientApps.Stop (Seconds (args.endTimeBase));

  Simulator::Stop (Seconds (args.endTimeBase));
  
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
}
