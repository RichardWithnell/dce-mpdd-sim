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

struct ModelNode;

struct ModelNode {
    float degMin;
    float degMax;
    float degInc;
    float degree;
    float x;
    float y;
};

#define POLAR_TOTAL 360.00f
#define POLAR_MAX 180.00f
#define VECTOR_LENGTH 10.00f

/**
x = r cos (theta),
y = r sin (theta),
**/
#define PI 3.141592653589793f
#define RAD(x) x * PI / 180.00f
#define DO_ROUND 0
#if DO_ROUND
#define ROUND(d) floor(d+0.5)
#else
#define ROUND(d) d
#endif
int build_model(int nDevices, int nStride, Ptr<ListPositionAllocator> positionAlloc)
{
    int j = 0;
    float initX = 0.00;
    float initY = 0.00;
    float degree_between = nStride/POLAR_TOTAL;
    float start_degree = 0;
    float vectorMulti = 5.00;
    float vectorLength = (get_height(nStride, nDevices)+1)*vectorMulti;
    std::vector<struct ModelNode> nodeVector;

    for (long i = 0; i < nDevices; i++) {
        if(i == 0){
            struct ModelNode n;
            n.x = initX;
            n.y = initY;
            n.degMax = POLAR_TOTAL;
            n.degMin = 0;
            n.degree = 0;
            n.degInc = POLAR_TOTAL / (float)(nStride <= nDevices ? nStride : nDevices);
            std::cout << "DEG INC " << n.degInc << "\n";

            nodeVector.push_back(n);
        } else {
            struct ModelNode n;
            struct ModelNode pNode;

            int p = get_parent(i, nStride);
            std::cout << "NODE: " << i << "\n";

            int c = get_first_child(p, nStride);
            int child_index = i - c;
            std::cout << "\tChild Index: " << child_index << "\n";

            int l = c+nStride;

            pNode = nodeVector.at(p);

            std::cout << "\tParent Deg Min: " << pNode.degMin << "\n";
            std::cout << "\tParent Deg Inc: " << pNode.degInc << "\n";

            float degree = pNode.degMin + (pNode.degInc * child_index);
            std::cout << "\tDegree: " << degree << "\n";

            n.x = ROUND(pNode.x + (vectorLength*cos(RAD(degree))));
            n.y = ROUND(pNode.y + (vectorLength*sin(RAD(degree))));

            std::cout << "\tSTAR(" << n.x << ", " << n.y << ")\n";

            n.degMax = degree+(POLAR_MAX/2);
            n.degMin = degree-(POLAR_MAX/2);
            n.degInc = POLAR_MAX/(nStride);

            //float tmp = n.degMax - (n.degMin + (nStride*n.degInc));
            //n.degMin += tmp/2;

            nodeVector.push_back(n);


        }
        if((i % nStride) == 0) {
            vectorLength = vectorLength - (vectorLength  * 0.095);
            if(vectorLength < vectorMulti) vectorLength = vectorMulti;
        }
    }

    for(std::vector<struct ModelNode>::size_type i = 0; i != nodeVector.size(); i++) {
        std::cout << "\tSTAR(" << nodeVector[i].x << ", " << nodeVector[i].y << ")\n";
        positionAlloc->Add (ns3::Vector (nodeVector[i].x,  nodeVector[i].y, 0.0));
    }

    return 0;
}

int main(int argc, char *argv[])
{
    std::string wifiBaseName = "WiFi";

    DceApplicationHelper appHelper;
    MpddHelper dce;
    DceManagerHelper dceManager;
    ApplicationContainer apps;
    ApplicationContainer confapps;
    LinuxStackHelper stack;
    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    std::vector< Ptr<YansWifiChannel> > channels;

    MobilityHelper mobility;
    WifiHelper wifi = WifiHelper::Default();
    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();

    std::stringstream cmdStream;

    uint32_t nDevices = 7;
    uint32_t treeStride = 2;
    uint32_t nInterfaces = 0;
    uint32_t nRootInterfaces = 2;

    CommandLine cmd;
    cmd.AddValue("devices", "Number of wifi devices", nDevices);
    cmd.AddValue("stride", "Tree Stride", treeStride);
    cmd.AddValue("interfaces", "Number of gateway interfaces for non root devices", nInterfaces);
    cmd.AddValue("root_interfaces", "Number of gateway interfaces for root device", nRootInterfaces);

    cmd.Parse(argc, argv);

    //GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));

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


    build_model(nDevices, treeStride, positionAlloc);

    /****
    * Setup tree structure
    ****/
    for (int i = 0; i < nodes.GetN(); i++) {
        int k = 0;
        std::stringstream tempWifiName;
        tempWifiName << "" << wifiBaseName << "-" << i;
        Ssid apSsid = Ssid(tempWifiName.str());

        Ptr<YansWifiChannel> chan = wifiChannel.Create();

        uint32_t firstChild = get_first_child(i, treeStride);
        for(int j = 0; j < treeStride; j++){
            if((firstChild+j) >= nodes.GetN()) break;
            k++;
        }

        /*Avoid adding WiFi AP's if there won't be any nodes connecting*/
        if(k > 0){
            wifiPhy.SetChannel(chan);
            channels.push_back(chan);

            wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(apSsid));
            NetDeviceContainer dev = wifi.Install(wifiPhy, wifiMac, nodes.Get(i));
            apDevices.Add(dev);

        }
        /*Ignore root interfaces*/
        if(i != 0){
            uint32_t parent = get_parent(i, treeStride);
            //std::cout << "Parent of " << i << " is " << parent << std::endl;
            tempWifiName.str(std::string());
            tempWifiName << "" << wifiBaseName << "-" << parent;
            //std::cout << tempWifiName.str() << std::endl;

            wifiPhy.SetChannel(channels.at(parent));
            Ssid staSsid = Ssid(tempWifiName.str());
            wifiMac.SetType("ns3::StaWifiMac",
                             "Ssid", SsidValue(staSsid),
                             "ActiveProbing", BooleanValue(false));
            NetDeviceContainer dev = wifi.Install(wifiPhy, wifiMac, nodes.Get(i));
            staDevices.Add(dev);
        }
    }



    mobility.SetPositionAllocator (positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    /****
    * Setup Addresses
    ****/

    for (int i = 0; i < nodes.GetN(); i++) {
        std::stringstream cmd;

        uint32_t firstChild = get_first_child(i, treeStride);

        cmd << "link set dev sim0 up";
        LinuxStackHelper::RunIp (nodes.Get (i), Seconds (1), cmd.str());
        cmd.str(std::string());
        cmd << "addr add 10.1." << i + 1 << ".1/24 broadcast 10.1." << i + 1 << ".255 dev sim0";

        std::cout << "NODE " << i + 1 << ": " << cmd.str() << std::endl;

        LinuxStackHelper::RunIp (nodes.Get (i), Seconds (2), cmd.str());

        std::cout << "FirstChild: " << firstChild << "\n";
        for(int j = 0; j < treeStride; j++){
            if((firstChild + j) >= nodes.GetN()){
                std::cout << "FirstChild: " << firstChild << " Nodes: " << nodes.GetN() << "\n";
                break;
            }

            cmd.str(std::string());
            cmd << "link set dev sim1 up";
            LinuxStackHelper::RunIp (nodes.Get (firstChild+j), Seconds (1), cmd.str());

            cmd.str(std::string());
            cmd << "addr add 10.1." << i + 1 << "." << firstChild + j + 1 << "/24 broadcast 10.1." << i + 1 << ".255 dev sim1";
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

        dce.DisseminationInterface("sim0");
        std::cout << "Set diss to sim0\n";

        apps = dce.InstallInNode(nodes.Get(i), 2, "node");
        std::cout << "Installed in " << i << "\n";
    }

    apps.Start(Seconds (5));
    std::cout << "DONE\n";

    wifiPhy.EnablePcap("dce-mpdd-nested-wifi", apDevices, true);

    //pointToPoint.EnablePcapAll("dce-mpdd-nested-ptp", true);

    Simulator::Stop(Seconds(15));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
