#include "ns3/dce-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    DceApplicationHelper dce;
    DceManagerHelper dceManager;
    ApplicationContainer apps;
    LinuxStackHelper stack;

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (100)));


    NodeContainer nodes;

    nodes.Create(2);

    NetDeviceContainer devices = csma.Install(nodes);


    dceManager.SetTaskManagerAttribute ("FiberManagerType", StringValue ("UcontextFiberManager"));
    dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux.so"));

    stack.Install (nodes);

    dceManager.Install (nodes);

    dce.SetStackSize (1 << 20);
    dce.SetBinary ("simple_link_monitor");
    dce.ResetArguments ();
    dce.ResetEnvironment ();
    apps = dce.Install(nodes);
    apps.Start (Seconds (1));

    std::stringstream cmd;
    cmd << "link set dev sim0 up";
    LinuxStackHelper::RunIp (nodes.Get(1), Seconds (0.1), cmd.str());
    cmd.str(std::string());
    cmd << "addr add 192.168.1.10/24 broadcast 192.168.1.255 dev sim0";
    LinuxStackHelper::RunIp (nodes.Get(1), Seconds (10), cmd.str());
    cmd.str(std::string());
    cmd << "route add default via 192.168.1.1 dev sim0";
    LinuxStackHelper::RunIp (nodes.Get(1), Seconds (12), cmd.str());

    cmd.str(std::string());
    cmd << "link set dev sim0 up";
    LinuxStackHelper::RunIp (nodes.Get(0), Seconds (0.1), cmd.str());
    cmd.str(std::string());
    cmd << "addr add 192.168.1.10/24 broadcast 192.168.1.255 dev sim0";
    LinuxStackHelper::RunIp (nodes.Get(0), Seconds (10), cmd.str());
    cmd.str(std::string());
    cmd << "route add default via 192.168.1.1 dev sim0";
    LinuxStackHelper::RunIp (nodes.Get(0), Seconds (12), cmd.str());

    Simulator::Stop (Seconds (30.00));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}
