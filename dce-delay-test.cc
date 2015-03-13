#include <fstream>
#include "ns3/core-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/dce-module.h"
#include "ns3/point-to-point-module.h"
#include <unistd.h>

using namespace ns3;

Ptr<NormalRandomVariable> createNormalRandomVariable(double mean, double variance, double bound){
    Ptr<NormalRandomVariable> x = CreateObject<NormalRandomVariable> ();
    x->SetAttribute ("Mean", DoubleValue (mean));
    x->SetAttribute ("Variance", DoubleValue (variance));
    x->SetAttribute ("Bound", DoubleValue (bound));
    return x;
}

bool netDevCb(
  Ptr<NetDevice> device,
  Ptr<const Packet> pkt,
  uint16_t  protocol,
  const Address &from)
{
    static Ptr<NormalRandomVariable> x = createNormalRandomVariable(100, 100, 50);
    Ptr<Node> node = device->GetNode ();
    Simulator::Schedule(MilliSeconds(x->GetValue()), &Node::NonPromiscReceiveFromDevice, node, device, pkt, protocol, from);
    return  true;
}

int
main (int argc, char *argv[])
{
    Address serverAddress;
    DceManagerHelper dceManager;
    DceApplicationHelper dce;
    ApplicationContainer apps;
    Ipv4AddressHelper address;
    PointToPointHelper ptpHelper;
    InternetStackHelper stack;

    NodeContainer nodes;
    nodes.Create (2);

    ptpHelper.SetDeviceAttribute ("DataRate", StringValue ("1Gb/s"));
    ptpHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
    NetDeviceContainer devices = ptpHelper.Install (nodes.Get(0), nodes.Get(1));

    stack.Install (nodes);
    address.SetBase ("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    dceManager.Install (nodes);

    Ptr<NetDevice> serverDevice = devices.Get(1);

    Callback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address&> cb;
    cb = MakeCallback(&netDevCb);
    serverDevice->SetReceiveCallback(cb);

    dce.SetStackSize (1 << 20);

    dce.SetBinary ("ping");
    dce.ResetArguments ();
    dce.ResetEnvironment ();
    dce.AddArgument ("10.1.1.2");

    apps = dce.Install (nodes.Get (0));
    apps.Start (Seconds (1.00));

    std::cout << "Start.\n";

    Simulator::Stop (Seconds (10.00));
    Simulator::Run ();
    Simulator::Destroy ();

}
