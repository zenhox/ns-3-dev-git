/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-grid.h"
#include <iostream>
using namespace std;

// Default Network Topology
//
//       10.1.1.0
// n0 -------------- n1
//    point-to-point
//
 
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");

struct Args{
  unsigned int xSize;
  unsigned int ySize;
  unsigned int pktNum;
  unsigned int pktInterval;
  unsigned int pktSize;
  unsigned int stopTime;
};


void test1(Args arg){

  int x = arg.xSize;
  int y = arg.ySize;

  PointToPointHelper  pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
 // Creating 3x3 topology
  PointToPointGridHelper grid(x, y, pointToPoint);
  grid.BoundingBox(100, 100, 200, 200);

  InternetStackHelper stack;
  grid.InstallStack(stack);

  grid.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                            Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"));

  for(int i=0; i< x; ++i){
    for(int j=0; j<y ; ++j){
      UdpEchoServerHelper echoServer (9);
      ApplicationContainer serverApps = echoServer.Install (grid.GetNode (i,j));
      serverApps.Start (Seconds (1.0));
      serverApps.Stop (Seconds(arg.stopTime));
      for(int ii=0; ii<x; ++ii){
        for(int jj=0; jj<y; ++jj){
          if(ii ==i && jj ==j){
            continue;
          }
          UdpEchoClientHelper echoClient (grid.	GetIpv4Address(ii,jj), 9);
          echoClient.SetAttribute ("MaxPackets", UintegerValue(arg.pktNum));
          echoClient.SetAttribute ("Interval", TimeValue(MilliSeconds(arg.pktInterval)) );
          echoClient.SetAttribute ("PacketSize",UintegerValue(arg.pktSize));
          ApplicationContainer clientApps = echoClient.Install (grid.GetNode(i,j));
          clientApps.Start (Seconds (i+j));
          clientApps.Stop (Seconds(arg.stopTime));
        }
      }
    }
  }
  Simulator::Run ();
  cout << "Event count = " << Simulator::GetEventCount() << endl;
  Simulator::Destroy ();
}

int
main (int argc, char *argv[])
{
  Args argDefault;
  argDefault.xSize = 5;
  argDefault.ySize = 5;
  argDefault.pktNum = 10;
  argDefault.pktInterval = 100;
  argDefault.pktSize = 1024;
  argDefault.stopTime = 3000;
  
  Time::SetResolution (Time::NS);
  // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  GlobalValue::Bind ("SimulatorImplementationType",
                    StringValue ("ns3::HSPSimulatorImpl"));

  test1(argDefault);
  return 0;
}
