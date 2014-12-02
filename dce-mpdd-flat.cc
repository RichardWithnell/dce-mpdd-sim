#include "ns3/dce-module.h"

#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"

#include "ns3/ipv4-global-routing-helper.h"

#include "ns3/udp-client-server-helper.h"
#include "ns3/udp-echo-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nDevices = 1;

    CommandLine cmd;
    cmd.AddValue("devices", "Number of wifi devices", nDevices);

    cmd.Parse(argc, argv);

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

    wifiPhy.SetChannel(wifiChannel.Create());

    wifiMac.SetType("ns3::StaWifiMac",
                     "Ssid", SsidValue(ssid),
                     "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, apNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(5.0),
        "DeltaY", DoubleValue(10.0),
        "GridWidth", UintegerValue(3),
        "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);
    mobility.Install(apNode);

    InternetStackHelper internet;
    internet.Install(staNodes);
    internet.Install(apNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    NetDeviceContainer devices;
    DceManagerHelper dceManager;

    ApplicationContainer apps;
    DceApplicationHelper dce;


    dceManager.Install(staNodes);
    dceManager.Install(apNode);

    dce.SetStackSize(1<<20);

    dce.SetBinary("udp-server");
    dce.ResetArguments();
    apps = dce.Install(apNode.Get(0));
    apps.Start(Seconds(4.0));

    dce.SetBinary("udp-client");
    dce.ResetArguments();
    dce.AddArgument("10.1.1.1");
    apps = dce.Install(staNodes.Get(0));
    apps.Start(Seconds(4.5));


    Ipv4GlobalRoutingHelper::PopulateRoutingTables();


    Simulator::Stop(Seconds(15.05));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
