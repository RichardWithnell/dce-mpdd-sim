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

    uint32_t nDevices = 3;
    uint32_t treeStride = 2;
    uint32_t nInterfaces = 0;
    uint32_t nRootInterfaces = 2;

    CommandLine cmd;
    cmd.AddValue("devices", "Number of wifi devices", nDevices);
    cmd.AddValue("stride", "Tree Stride", treeStride);
    cmd.AddValue("interfaces", "Number of gateway interfaces for non root devices", nInterfaces);
    cmd.AddValue("root_interfaces", "Number of gateway interfaces for root device", nRootInterfaces);

    cmd.Parse(argc, argv);

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (10000000)));


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
    stack.Install (allHosts);
#else
    NS_LOG_ERROR ("Linux kernel stack for DCE is not available. build with dce-linux module.");
    return 0;
#endif

    /****
    * Setup tree structure
    ****/
    for (long i = 0; i < nodes.GetN(); i++) {
        NodeContainer tempNodes;

        uint32_t firstChild = get_first_child(i, treeStride);

        tempNodes.Add(nodes.Get(i));

        for(int j = 0; j < treeStride; j++){
            if((firstChild+j) >= nodes.GetN()) break;
            tempNodes.Add(nodes.Get(firstChild+j));
        }

        NetDeviceContainer devices = csma.Install(tempNodes);
        apDevices.Add(devices.Get(0));
        for(int j = 1; j <= treeStride; j++){
            if((firstChild+j) >= nodes.GetN()) break;
            staDevices.Add(devices.Get(j));
        }
    }

    /****
    * Setup Addresses
    ****/
    for (long i = 0; i < nodes.GetN(); i++) {
        std::stringstream tempAddress;
        std::stringstream cmd;
        Ipv4AddressHelper address;
        NetDeviceContainer apDev = apDevices.Get(i);
        tempAddress << "192.168." << i << ".0";
        address.SetBase(tempAddress.str().c_str(), "255.255.255.0");
        address.Assign(apDev);

        cmd << "link set dev sim0 up";
        LinuxStackHelper::RunIp (nodes.Get (i), Seconds (0.1), cmd.str());

        uint32_t firstChild = get_first_child(i, treeStride);

        for(int j = 0; j < treeStride; j++){
            if((firstChild+j-1) >= staDevices.GetN()) break;
            NetDeviceContainer staDev = staDevices.Get(firstChild+j-1);
            address.Assign(staDev);
        }
    }

    /**
    * Setup Routing
    **/

    for (long i = 0; i < nodes.GetN(); i++) {
        std::stringstream cmd;
        int parent = get_parent(i, treeStride);
        if(parent >= 0){
            cmd << "route add default via 192.168." << parent << ".1 dev sim0";
            //std::cout << "Adding to Node: " << i << std::endl;
            //std::cout << cmd.str() << std::endl;
            LinuxStackHelper::RunIp (nodes.Get (i), Seconds (0.1), cmd.str());
            cmd.str(std::string());
            cmd << "route add 192.168." << i << ".0/24 dev sim1";
            LinuxStackHelper::RunIp (nodes.Get (i), Seconds (0.1), cmd.str());
            cmd.str(std::string());
            cmd << "route add 192.168." << parent << ".0/24 dev sim0";
            LinuxStackHelper::RunIp (nodes.Get (i), Seconds (0.1), cmd.str());

            /*Setup NAT*/
            cmd.str(std::string());
            cmd << "route add 192.168." << parent << ".0/24 dev sim0";
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
      Ipv4AddressHelper address;
      tempAddress << "172.16." << i << ".0";
      cmd << "route add default via 172.16." << i << ".2 dev sim0" << std::endl;;
      LinuxStackHelper::RunIp (nodes.Get(0), Seconds (1), cmd.str());

      address.SetBase(tempAddress.str().c_str(), "255.255.255.0");
      address.Assign(dev);
      gatewayDevices.Add(dev);
    }

    /****
    *
    * Setup the MPDD
    *
    ****/

    //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    //LinuxStackHelper::PopulateRoutingTables ();

    dce.SetStackSize(1 << 20);
    appHelper.SetStackSize(1 << 20);

    stack.SysctlSet(nodes, ".net.ipv4.conf.all.forwarding", "1");

    for (long i = 0; i < nodes.GetN(); i++) {
        LinuxStackHelper::RunIp (nodes.Get(i), Seconds (2), "route show");

        LinuxStackHelper::RunIp (nodes.Get(i), Seconds (2), "addr");
    }

    /**/
    appHelper.SetBinary ("xtables-multi");
    appHelper.ResetArguments ();
    appHelper.ResetEnvironment ();
    appHelper.AddArgument ("iptables");
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


    dce.SetBinary("mpdd");
    dce.ResetArguments();
    dce.ResetEnvironment();
    dce.AddArgument("-C");
    dce.AddArgument("/etc/mpd/mpdd.conf");
    dce.IgnoreInterface("lo");
    dce.DisseminationInterface("sim1");
    /*2 creates a simple config file, that doens't use libconfig. "node" is an ID.*/
    apps = dce.Install(nodes, 2, "node");

    apps.Start(Seconds (3));

    //csma.EnablePcapAll("mpdd-nested");

    Simulator::Stop(Seconds(10.00));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
