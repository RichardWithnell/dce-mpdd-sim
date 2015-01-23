#include "ns3/dce-module.h"
#include "ns3/core-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"

#include <math.h>
#include <string>
#include <sstream>

using namespace ns3;

int main(int argc, char *argv[])
{
    DceApplicationHelper dce;
    DceManagerHelper dceManager;
    ApplicationContainer apps;
    LinuxStackHelper stack;
    Ipv4AddressHelper ipv4Addr;

    uint32_t nDevices = 2;

    NodeContainer nodes;

    nodes.Create(nDevices);

    dceManager.SetTaskManagerAttribute("FiberManagerType", StringValue ("UcontextFiberManager"));
    dceManager.SetNetworkStack("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux.so"));
    dceManager.Install(nodes);

    Ipv4DceRoutingHelper ipv4RoutingHelper;
    stack.SetRoutingHelper (ipv4RoutingHelper);

    stack.SysctlSet(nodes, ".net.ipv6.conf.all.disable_ipv6", "1");

    stack.Install (nodes);

    CsmaHelper csma;

    NetDeviceContainer c = csma.Install(nodes);
    ipv4Addr.SetBase ("10.1.1.0", "255.255.255.0");
    ipv4Addr.Assign (c);

    LinuxStackHelper::PopulateRoutingTables ();

    dce.SetStackSize(1 << 20);

    stack.SysctlSet(nodes, ".net.ipv6.conf.all.disable_ipv6", "1");


    dce.SetBinary ("udp-server");
    apps = dce.Install (nodes.Get(0));
    apps.Start (Seconds (5.0));


    dce.ResetArguments();
    dce.SetBinary ("broadcast");
    dce.AddArgument("10.1.1.255");
    apps = dce.Install (nodes.Get(1));
    apps.Start (Seconds (5.5));

    csma.EnablePcapAll ("dce-broadcast");

    LinuxStackHelper::RunIp (nodes.Get(0), Seconds (1), "route show table main");
    LinuxStackHelper::RunIp (nodes.Get(0), Seconds (1), "route show table local");
    LinuxStackHelper::RunIp (nodes.Get(0), Seconds (1), "addr show");

    LinuxStackHelper::RunIp (nodes.Get(1), Seconds (1), "route show table main");
    LinuxStackHelper::RunIp (nodes.Get(1), Seconds (1), "route show table local");
    LinuxStackHelper::RunIp (nodes.Get(1), Seconds (1), "addr show");

    Simulator::Stop(Seconds(1010.00));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
