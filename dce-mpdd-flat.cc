
#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"

#include "ns3/ipv4-global-routing-helper.h"

#include "ns3/udp-client-server-helper.h"
#include "ns3/udp-echo-helper.h"

#include "ns3/dce-module.h"


using namespace ns3;

void setPos (Ptr<Node> n, int x, int y, int z)
{
  Ptr<ConstantPositionMobilityModel> loc = CreateObject<ConstantPositionMobilityModel> ();
  n->AggregateObject (loc);
  Vector locVec2 (x, y, z);
  loc->SetPosition (locVec2);
}

int main(int argc, char *argv[])
{
    uint32_t nDevices = 0;

    CommandLine cmd;
    cmd.AddValue("devices", "Number of wifi devices", nDevices);

    cmd.Parse(argc, argv);

    if (nDevices == 0){
        nDevices = 1;
    }

    NodeContainer staNodes;
    NodeContainer apNode;

    staNodes.Create(nDevices);
    apNode.Create(1);

    Ssid ssid = Ssid("wifi-default");
    WifiHelper wifi = WifiHelper::Default();
    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();

    wifi.SetRemoteStationManager("ns3::ArfWifiManager");
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

    wifiPhy.SetChannel(wifiChannel.Create());

    wifiMac.SetType("ns3::StaWifiMac",
                     "Ssid", SsidValue(ssid),
                     "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);
    NetDeviceContainer devices = staDevices;

    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, apNode);
    devices.Add(apDevices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
      "MinX", DoubleValue(-5.0),
      "MinY", DoubleValue(-5.0),
      "DeltaX", DoubleValue(-5.0),
      "DeltaY", DoubleValue(-5.0),
      "GridWidth", UintegerValue(3),
      "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
    mobility.Install(staNodes);

    mobility.Install(apNode);

    InternetStackHelper internet;
    internet.Install(staNodes);
    internet.Install(apNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0", "0.0.0.1");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);



    DceManagerHelper dceManager;

    ApplicationContainer apps;
    DceApplicationHelper dce;

    dceManager.Install(staNodes);
    dceManager.Install(apNode);

    dce.SetStackSize(1<<20);

    dce.SetBinary ("iperf");
    dce.ResetArguments ();
    dce.ResetEnvironment ();
    dce.AddArgument ("-c");
    dce.AddArgument ("192.168.1.1");
    dce.ParseArguments ("-y C");
    dce.AddArgument ("-i");
    dce.AddArgument ("1");
    dce.AddArgument ("--time");
    dce.AddArgument ("10");

    apps = dce.Install (staNodes.Get(0));
    apps.Start (Seconds (5.0));


    // Launch iperf server on node 1
    dce.SetBinary ("iperf");
    dce.ResetArguments ();
    dce.ResetEnvironment ();
    dce.AddArgument ("-s");
    dce.AddArgument ("-P");
    dce.AddArgument ("1");

    apps = dce.Install (apNode.Get(0));
    apps.Start (Seconds (1.0));


    Simulator::Stop(Seconds(30.00));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
