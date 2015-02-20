#include "ns3/dce-module.h"

#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"

#include "ns3/ipv4-global-routing-helper.h"

#include "ns3/udp-client-server-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/log.h"

#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

#include <math.h>
#include <string>
#include <sstream>


using namespace ns3;



uint32_t
get_height(uint32_t stride, uint32_t nodes)
{
    /*h = logk(k-1) + logk(n) -1*/
    return (log(stride-1) / log(stride)) + (log(nodes) / log(stride));
}

uint32_t
get_leaves(uint32_t stride, uint32_t height)
{
    return pow(stride, height);
}

int
get_parent(int node, int stride)
{
    if(node <= 0) return -1;
    return (uint32_t)(floor((node-1)/stride));
}

uint32_t
get_first_child(int node, int stride)
{
    return (uint32_t)(node*stride)+1;
}

uint32_t
is_leaf(uint32_t stride, uint32_t nodes, uint32_t nodeIdx)
{
    uint32_t height = get_height(stride, nodes);
    uint32_t leaves = get_leaves(stride, height);

    return (nodes-leaves) < nodeIdx ? 1 : 0;
}

int main(int argc, char *argv[])
{
    DceApplicationHelper appHelper;
    MpddHelper dce;
    DceManagerHelper dceManager;
    ApplicationContainer apps;
    ApplicationContainer confapps;
    LinuxStackHelper stack;
    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;

    std::stringstream cmdStream;

    uint32_t nDevices = 7;
    uint32_t treeStride = 2;
    uint32_t nInterfaces = 0;
    uint32_t nRootInterfaces = 2;

    CommandLine cmd;
    cmd.AddValue("devices", "Number of wifi devices", nDevices);
    cmd.AddValue("stride", "Tree Stride", treeStride);
    cmd.AddValue("interfaces",
        "Number of gateway interfaces for non root devices", nInterfaces);
    cmd.AddValue("root_interfaces",
        "Number of gateway interfaces for root device", nRootInterf);

    cmd.Parse(argc, argv);

    //GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));

    NodeContainer nodes, routers, allHosts;

    nodes.Create(nDevices);
    routers.Create(nRootInterfaces);
    allHosts.Add(nodes);
    allHosts.Add(routers);

    /**
    * Setup Linux Stack
    **/
    dceManager.SetTaskManagerAttribute ("FiberManagerType", StringValue ("UcontextFiberManager"));

    #ifdef KERNEL_STACK
    dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux.so"));
    dceManager.Install (allHosts);

    Ipv4DceRoutingHelper ipv4RoutingHelper;
    stack.SetRoutingHelper (ipv4RoutingHelper);
    stack.Install (allHosts);
    #else
    NS_LOG_ERROR ("Linux kernel stack for DCE is not available. build with dce-linux module.");
    return 0;
    #endif

    /****
    * Setup tree structure
    ****/
    for (int i = 0; i < nodes.GetN(); i++) {
        NodeContainer tempNodes;

        uint32_t firstChild = get_first_child(i, treeStride);

        tempNodes.Add(nodes.Get(i));

        for(int j = 0; j < treeStride; j++){
            if((firstChild+j) >= nodes.GetN()) break;
            tempNodes.Add(nodes.Get(firstChild+j));
        }

        if(tempNodes.GetN() > 1){
            NetDeviceContainer devices = csma.Install(tempNodes);
            apDevices.Add(devices.Get(0));

            for(int j = 1; j <= treeStride; j++){
                if((firstChild+j) >= nodes.GetN()) break;
                staDevices.Add(devices.Get(j));
            }
        }
    }

    /****
    * Setup Addresses
    ****/

    for (int i = 0; i < nodes.GetN(); i++) {
        std::stringstream cmd;

        uint32_t firstChild = get_first_child(i, treeStride);

        if(i == 0){
            cmd << "link set dev sim0 up";
            LinuxStackHelper::RunIp (nodes.Get (i), Seconds (1), cmd.str());
            cmd.str(std::string());
            cmd << "addr add 10.1." << i + 1 << ".1/24 broadcast 10.1." << i + 1 << ".255 dev sim0";
        } else {
            cmd << "link set dev sim1 up";
            LinuxStackHelper::RunIp (nodes.Get (i), Seconds (1), cmd.str());
            cmd.str(std::string());
            cmd << "addr add 10.1." << i + 1 << ".1/24 broadcast 10.1." << i + 1 << ".255 dev sim1";
        }
        std::cout << "NODE " << i + 1 << ": " << cmd.str() << std::endl;

        LinuxStackHelper::RunIp (nodes.Get (i), Seconds (2), cmd.str());

        std::cout << "FirstChild: " << firstChild << "\n";
        for(int j = 0; j < treeStride; j++){
            if((firstChild + j) >= nodes.GetN()){
                std::cout << "FirstChild: " << firstChild << " Nodes: " << nodes.GetN() << "\n";
                break;
            }

            cmd.str(std::string());
            cmd << "link set dev sim0 up";
            LinuxStackHelper::RunIp (nodes.Get (firstChild+j), Seconds (1), cmd.str());

            cmd.str(std::string());
            cmd << "addr add 10.1." << i + 1 << "." << firstChild + j + 1 << "/24 broadcast 10.1." << i + 1 << ".255 dev sim0";
            std::cout << "NODE " << firstChild + j + 1 << ": " << cmd.str() << std::endl;
            LinuxStackHelper::RunIp (nodes.Get (firstChild+j), Seconds (2), cmd.str());
        }
    }


    /**
    * Setup Routing
    **/

    for (int i = 0; i < nodes.GetN(); i++) {
        std::stringstream cmd;
        int parent = get_parent(i, treeStride) + 1;
        if(parent >= 1) {
            //cmd << "route add default via 10.1." << parent << ".1 dev sim0";
            //std::cout << "Adding to Node: " << i << std::endl;
            //std::cout << cmd.str() << std::endl;
            //LinuxStackHelper::RunIp (nodes.Get (i), Seconds (4), cmd.str());
            //cmd.str(std::string());
            cmd << "route add 10.1." << i + 1 << ".0/24 dev sim1";
            std::cout << "NODE " << i + 1 << ": " << cmd.str() << std::endl;
            LinuxStackHelper::RunIp (nodes.Get (i), Seconds (3), cmd.str());
            cmd.str(std::string());
            cmd << "route add 10.1." << parent << ".0/24 dev sim0";
            std::cout << "NODE " << i + 1 << ": " << cmd.str() << std::endl;
            LinuxStackHelper::RunIp (nodes.Get (i), Seconds (3), cmd.str());
        } else {
            cmd << "route add 10.1.1.0/24 dev sim0";
            std::cout << "NODE " << i + 1 << ": " << cmd.str() << std::endl;
            LinuxStackHelper::RunIp (nodes.Get (i), Seconds (3), cmd.str());
        }
    }

    NetDeviceContainer gatewayDevices;

    /*Add Gateway Routers*/
    PointToPointHelper pointToPoint;
    //pointToPoint.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    //pointToPoint.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (30)));
    for (int i = 0; i < routers.GetN(); i++){
        std::stringstream cmd;
        std::stringstream tempAddress;

        NetDeviceContainer dev = pointToPoint.Install(nodes.Get(0), routers.Get(i));

        cmd << "link set dev sim" << i + 1 << " up";
        LinuxStackHelper::RunIp (nodes.Get (0), Seconds (1), cmd.str());

        cmd.str(std::string());
        cmd << "link set dev sim0 up";
        LinuxStackHelper::RunIp (routers.Get(i), Seconds (1), cmd.str());

        cmd.str(std::string());
        cmd << "addr add 192.168." << i + 1 << ".1/24 broadcast 192.168." << i + 1 << ".255 dev sim" << i + 1;
        std::cout << "NODE 1: " << cmd.str() << std::endl;
        LinuxStackHelper::RunIp (nodes.Get(0), Seconds (2), cmd.str());

        cmd.str(std::string());
        cmd << "addr add 192.168." << i + 1 << ".2/24 broadcast 192.168." << i + 1 << ".255 dev sim0";
        std::cout << "Router " << i + 1 << ": " << cmd.str() << std::endl;
        LinuxStackHelper::RunIp (routers.Get(i), Seconds (2), cmd.str());

        cmd.str(std::string());
        tempAddress << "192.168." << i + 1 << ".0";
        cmd << "route add default via 192.168." << i + 1 << ".2 dev sim" << i + 1 << " metric " << i + 1;

        std::cout << "NODE " << 1 << ": " << cmd.str() << std::endl;

        gatewayDevices.Add(dev);

        LinuxStackHelper::RunIp (nodes.Get(0), Seconds (10), cmd.str());

    }

    //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    //LinuxStackHelper::PopulateRoutingTables ();

    dce.SetStackSize(1 << 20);
    appHelper.SetStackSize(1 << 20);

    stack.SysctlSet(nodes, ".net.ipv4.conf.all.forwarding", "1");
    stack.SysctlSet(nodes, ".net.ipv6.conf.all.disable_ipv6", "1");

    for (int i = 0; i < allHosts.GetN(); i++) {
        LinuxStackHelper::RunIp (allHosts.Get(i), Seconds (4), "route show table main");
        LinuxStackHelper::RunIp (allHosts.Get(i), Seconds (4), "route show table local");
        LinuxStackHelper::RunIp (allHosts.Get(i), Seconds (4), "rule show");
        LinuxStackHelper::RunIp (allHosts.Get(i), Seconds (4), "addr show");
    }

    /*
    appHelper.SetBinary ("xtables-multi");
    appHelper.ResetArguments ();
    appHelper.ResetEnvironment ();
    appHelper.AddArgument ("iptables");
    appHelper.AddArgument ("-w");
    appHelper.AddArgument ("-t");
    appHelper.AddArgument ("nat");
    appHelper.AddArgument ("-A");
    appHelper.AddArgument ("POSTROUTING");
    appHelper.AddArgument ("-o");
    appHelper.AddArgument ("sim1");
    appHelper.AddArgument ("-j");
    appHelper.AddArgument ("MASQUERADE");
    confapps = appHelper.Install(nodes);
    confapps.Start(Seconds (0.5));

    appHelper.SetBinary ("xtables-multi");
    appHelper.ResetArguments ();
    appHelper.ResetEnvironment ();
    appHelper.AddArgument ("iptables");
    appHelper.AddArgument ("-w");
    appHelper.AddArgument ("-t");
    appHelper.AddArgument ("nat");
    appHelper.AddArgument ("-A");
    appHelper.AddArgument ("POSTROUTING");
    appHelper.AddArgument ("-o");
    appHelper.AddArgument ("sim0");
    appHelper.AddArgument ("-j");
    appHelper.AddArgument ("MASQUERADE");
    confapps = appHelper.Install(nodes);
    confapps.Start(Seconds (0.5));

    appHelper.SetBinary ("xtables-multi");
    appHelper.ResetArguments ();
    appHelper.ResetEnvironment ();
    appHelper.AddArgument ("iptables");
    appHelper.AddArgument ("-w");
    appHelper.AddArgument ("-t");
    appHelper.AddArgument ("nat");
    appHelper.AddArgument ("-A");
    appHelper.AddArgument ("POSTROUTING");
    appHelper.AddArgument ("-o");
    appHelper.AddArgument ("sim2");
    appHelper.AddArgument ("-j");
    appHelper.AddArgument ("MASQUERADE");
    confapps = appHelper.Install(nodes);
    confapps.Start(Seconds (0.5));
    */
    /****
    *
    * Setup the MPDD
    *
    ****/

    /*arg - 2 creates a simple config file, that doens't use libconfig. "node" is an ID.*/
    for (int i = 0; i < nodes.GetN(); i++) {
        dce.SetBinary("mpdd");
        dce.ResetArguments();
        dce.ResetEnvironment();

        dce.AddArgument("-C");
        dce.AddArgument("/etc/mpd/mpdd.conf");
        dce.IgnoreInterface("lo");
        dce.IgnoreInterface("sit0");
        dce.IgnoreInterface("ip6tnl0");

        if (i == 0){
            dce.DisseminationInterface("sim0");
            std::cout << "Set diss to sim0\n";
        } else {
            dce.DisseminationInterface("sim1");
            std::cout << "Set diss to sim1\n";
        }

        apps = dce.InstallInNode(nodes.Get(i), 2, "node");
        std::cout << "Installed in " << i << "\n";
    }

    apps.Start(Seconds (5));
    std::cout << "DONE\n";

    csma.EnablePcap("dce-mpdd-nested-csma", apDevices, true);

    //pointToPoint.EnablePcapAll("dce-mpdd-nested-ptp", true);

    Simulator::Stop(Seconds(15));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
