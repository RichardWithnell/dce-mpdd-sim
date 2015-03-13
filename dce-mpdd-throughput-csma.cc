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

void
PrintTcpFlags (std::string key, std::string value)
{
    std::cout << key << "=" << value;
}

#define DIST_GATEWAYS_YES 1
#define DIST_GATEWAYS_NO 0

#define MODE_TCP 0
#define MODE_TCP_LB 1
#define MODE_TCP_MPDP_LB 2
#define MODE_MPTCP 3
#define MODE_MPTCP_MPDP 4

void add_route_via(ns3::Ptr<ns3::Node> n, int this_node, int dev, int node_id, int stride, NodeContainer nodes)
{
    std::stringstream cmd;
    cmd.str(std::string());

    if (this_node < node_id){
        int cn = 0;
        int tmp_parent = get_parent(node_id, stride);

        cn = tmp_parent;
        /*is a child of a child*/
        while(tmp_parent > this_node) {
            tmp_parent = get_parent(tmp_parent, stride);
            if(tmp_parent > this_node) cn = tmp_parent;
            else break;
        }

        if(cn == this_node){
            cmd << "route add 10.1." << node_id + 1 << ".0/24 via 10.1." << this_node + 1 << "." << node_id + 1 << " dev sim" << dev << "";
        } else {
            cmd << "route add 10.1." << node_id + 1 << ".0/24 via 10.1." << this_node + 1 << "." << cn + 1 << " dev sim" << dev << "";
        }

        LinuxStackHelper::RunIp (n, Seconds (2), cmd.str());
    } else {
        cmd << "route add 10.1." << node_id + 1 << ".0/24 dev sim" << dev << "";
        LinuxStackHelper::RunIp (n, Seconds (2), cmd.str());
    }

    int child = get_first_child(node_id, stride);
    for(int i = 0; i < stride; i++){
        if((child  + i) >= nodes.GetN()) break;
        add_route_via(n, this_node, dev, child + i, stride, nodes);
    }
}

void add_route(ns3::Ptr<ns3::Node> n, int dev, int node_id, int stride, NodeContainer nodes)
{
    std::stringstream cmd;
    cmd.str(std::string());
    cmd << "route add 10.1." << node_id + 1 << ".0/24 dev sim" << dev << "";
    LinuxStackHelper::RunIp (n, Seconds (2), cmd.str());

    int child = get_first_child(node_id, stride);
    for(int i = 0; i < stride; i++){
        if((child  + i) >= nodes.GetN()) break;
        add_route(n, dev, child + i, stride, nodes);
    }
}

void start_nat(ns3::Ptr<ns3::Node> n, std::string interface)
{
    dce.SetBinary ("xtables-multi");
    dce.ResetArguments ();
    dce.ResetEnvironment ();
    dce.AddArgument ("iptables");
    dce.AddArgument ("-t");
    dce.AddArgument ("nat");
    dce.AddArgument ("-A");
    dce.AddArgument ("POSTROUTING");
    dce.AddArgument ("-o");
    dce.AddArgument (interface);
    dce.AddArgument ("-j");
    dce.AddArgument ("MASQUERADE");

    apps = dce.Install(n);
    apps.Start(Seconds(3));
}

int main(int argc, char *argv[])
{
    DceApplicationHelper appHelper;
    MpddHelper dce;
    DceManagerHelper dceManager;
    ApplicationContainer apps;
    ApplicationContainer confapps;
    ApplicationContainer iperfApps;
    LinuxStackHelper stack;
    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;
    NetDeviceContainer routerDevices;
    NetDeviceContainer serverDevices;

    NodeContainer nodes, routers, backbone, servers, serverGw, allHosts;
    std::stringstream cmdStream;

    std::map<int, std::vector<std::string> > gatewaysForNode;

    double stopTime = 80.0;
    std::string p2pdelay = "50ms";
    std::string iperfTime = "60";
    std::string ccalg = "reno";
    int flows = 1;
    int mode = MODE_TCP;
    int debug = 0;
    int delay = 0;

    uint32_t nDevices = 2;
    uint32_t treeStride = 2;
    uint32_t nInterfaces = 1;
    uint32_t nRootInterfaces = 0;
    uint32_t distributeGateways = 0;
    uint32_t nServers = 1;
    uint32_t nServerGw = 1;
    uint32_t iperfloc = 0;

    CommandLine cmd;
    cmd.AddValue("devices", "Number of wifi devices", nDevices);
    cmd.AddValue("stride", "Tree Stride", treeStride);
    cmd.AddValue("interfaces", "Number of gateway interfaces for non root devices", nInterfaces);
    cmd.AddValue("distribute_gateways", "Number of gateway interfaces for root device", distributeGateways);
    cmd.AddValue("mode", "Choose network/transport layer protocols. TCP/MPTCP/LB/MPDP", mode);
    cmd.AddValue("iperfloc", "Choose location of iperf (0) leaf, (1) all nodes", iperfloc);
    cmd.AddValue ("ccalg", "Set TCP Congestion Control Algorithm.", ccalg);
    cmd.AddValue ("delay", "Set variable delay on or off", delay);

    cmd.Parse(argc, argv);

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

    //backbone.Create(1);
    //servers.Create(nServers);
    nodes.Create(nDevices);
    routers.Create(nInterfaces);
    serverGw.Create(nServerGw);
    servers.Create(nServers);
    allHosts.Add(nodes);
    allHosts.Add(routers);
    allHosts.Add(servers);
    allHosts.Add(serverGw);

    std::cout << "Devices: \t" << nDevices << "\n";
    std::cout << "Stride: \t" << treeStride << "\n";
    std::cout << "Gateways: \t" << nInterfaces << "\n";
    std::cout << "Distribute: \t" << distributeGateways << "\n";
    std::cout << "Mode: \t\t" << mode << "\n";
    std::cout << "iPerf: \t\t" << iperfloc << "\n";

    std::cout << "\n\n";

    uint32_t dev_interfaces[nDevices];
    for(int i = 0; i < nDevices; i++) {
        dev_interfaces[i] = 1;
    }

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
            LinuxStackHelper::RunIp (nodes.Get (i), Seconds (2), cmd.str());
        } else {
            if(nodes.Get(i)->GetNDevices() > 1){
                cmd << "link set dev sim1 up";
                LinuxStackHelper::RunIp (nodes.Get (i), Seconds (1), cmd.str());
                cmd.str(std::string());
                cmd << "addr add 10.1." << i + 1 << ".1/24 broadcast 10.1." << i + 1 << ".255 dev sim1";
                LinuxStackHelper::RunIp (nodes.Get (i), Seconds (2), cmd.str());
            }
        }

        //std::cout << "FirstChild: " << firstChild << "\n";
        for(int j = 0; j < treeStride; j++){
            if((firstChild + j) >= nodes.GetN()){
                //std::cout << "FirstChild: " << firstChild << " Nodes: " << nodes.GetN() << "\n";
                break;
            }

            cmd.str(std::string());
            cmd << "link set dev sim0 up";
            LinuxStackHelper::RunIp (nodes.Get (firstChild+j), Seconds (1), cmd.str());

            cmd.str(std::string());
            cmd << "addr add 10.1." << i + 1 << "." << firstChild + j + 1 << "/24 broadcast 10.1." << i + 1 << ".255 dev sim0";
            //std::cout << "NODE " << firstChild + j + 1 << ": " << cmd.str() << std::endl;
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
            add_route_via(nodes.Get(i), i, 1, i, treeStride, nodes);

            cmd.str(std::string());
            cmd << "route add 10.1." << parent << ".0/24 dev sim0";
            std::cout << "NODE " << i << ": " << cmd.str() << std::endl;
            LinuxStackHelper::RunIp (nodes.Get (i), Seconds (3), cmd.str());

            if(mode == MODE_TCP){
                cmd.str(std::string());
                cmd << "route add default via 10.1." << parent << ".1 dev sim0";
                std::cout << "NODE " << i << ": " << cmd.str() << std::endl;
                LinuxStackHelper::RunIp (nodes.Get (i), Seconds (3), cmd.str());
            }
            if(mode == MODE_TCP_LB || mode == MODE_MPTCP){
                cmd.str(std::string());
                cmd << "route add default via 10.1." << parent << ".1 dev sim0 metric " << dev_interfaces[i];
                std::cout << "NODE " << i << ": " << cmd.str() << std::endl;
                LinuxStackHelper::RunIp (nodes.Get (i), Seconds (3), cmd.str());

                cmd.str(std::string());
                cmd << "rule add from 10.1." << parent << ".0/24 dev sim0 lookup " << dev_interfaces[i];
                std::cout << "NODE " << i << ": " << cmd.str() << std::endl;
                LinuxStackHelper::RunIp (nodes.Get (i), Seconds (3.5), cmd.str());

                cmd.str(std::string());
                cmd << "route add 10.1." << parent << ".0/24 dev sim0 table " << dev_interfaces[i];
                std::cout << "NODE " << i << ": " << cmd.str() << std::endl;
                LinuxStackHelper::RunIp (nodes.Get (i), Seconds (3), cmd.str());

                cmd.str(std::string());
                cmd << "route add default via 10.1." << parent << ".1 dev sim0 table " << dev_interfaces[i];
                std::cout << "NODE " << i << ": " << cmd.str() << std::endl;
                LinuxStackHelper::RunIp (nodes.Get (i), Seconds (3), cmd.str());

                dev_interfaces[i]++;
            }

        } else {
            add_route_via(nodes.Get(i), i, 0, i, treeStride, nodes);

            cmd << "route add 10.1.1.0/24 dev sim0";
            //std::cout << "NODE " << i + 1 << ": " << cmd.str() << std::endl;
            LinuxStackHelper::RunIp (nodes.Get (i), Seconds (3), cmd.str());
        }

    }

    NetDeviceContainer gatewayDevices;

    /*Add Gateway Routers*/
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

    for (int i = 0; i < routers.GetN(); i++){
        int nodeIdx = 0;
        if(distributeGateways == DIST_GATEWAYS_YES){
            nodeIdx = i % routers.GetN();
        }

        std::stringstream cmd;

        /*Connect network to the routers*/
        pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
        pointToPoint.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
        NetDeviceContainer dev = pointToPoint.Install(nodes.Get(nodeIdx), routers.Get(i));

        cmd << "link set dev sim" << nodes.Get (nodeIdx)->GetNDevices()-1 << " up";
        std::cout << "NODE " << nodeIdx << ": " << cmd.str() << std::endl;
        LinuxStackHelper::RunIp (nodes.Get (nodeIdx), Seconds (1), cmd.str());

        cmd.str(std::string());
        cmd << "addr add 192.168." << i + 1 << ".1/24 broadcast 192.168." << i + 1 << ".255 dev sim" << nodes.Get (nodeIdx)->GetNDevices()-1;
        std::cout << "NODE " << nodeIdx << ": " << cmd.str() << std::endl;
        LinuxStackHelper::RunIp (nodes.Get(nodeIdx), Seconds (2), cmd.str());

        cmd.str(std::string());
        cmd << "route add default via 192.168." << i + 1 << ".2 dev sim" << nodes.Get (nodeIdx)->GetNDevices()-1 << " metric " << dev_interfaces[nodeIdx];
        LinuxStackHelper::RunIp (nodes.Get(nodeIdx), Seconds (3), cmd.str());
        std::cout << "NODE " << nodeIdx << ": " << cmd.str() << std::endl;

        if(mode == MODE_TCP_LB || mode == MODE_MPTCP){
            cmd.str(std::string());
            cmd << "rule add from 192.168." << i + 1 << ".0/24 dev sim" << nodes.Get (nodeIdx)->GetNDevices()-1 << " lookup " << dev_interfaces[nodeIdx];
            std::cout << "NODE " << nodeIdx << ": " << cmd.str() << std::endl;
            LinuxStackHelper::RunIp (nodes.Get (nodeIdx), Seconds (3), cmd.str());

            cmd.str(std::string());
            cmd << "route add 192.168." << i + 1 << ".0/24 dev sim" << nodes.Get (nodeIdx)->GetNDevices()-1 << " table " << dev_interfaces[nodeIdx];
            std::cout << "NODE " << nodeIdx << ": " << cmd.str() << std::endl;
            LinuxStackHelper::RunIp (nodes.Get (nodeIdx), Seconds (3), cmd.str());

            cmd.str(std::string());
            cmd << "route add default via 192.168." << i + 1 << ".2 dev sim" << nodes.Get (nodeIdx)->GetNDevices()-1 << " table " << dev_interfaces[nodeIdx];
            std::cout << "NODE " << nodeIdx << ": " << cmd.str() << std::endl;
            LinuxStackHelper::RunIp (nodes.Get (nodeIdx), Seconds (3), cmd.str());
        }
        dev_interfaces[i]++;

        cmd.str(std::string());
        cmd << "link set dev sim0 up";
        LinuxStackHelper::RunIp (routers.Get(i), Seconds (1), cmd.str());

        cmd.str(std::string());
        cmd << "addr add 192.168." << i + 1 << ".2/24 broadcast 192.168." << i + 1 << ".255 dev sim0";
        //std::cout << "Router " << i + 1 << ": " << cmd.str() << std::endl;
        LinuxStackHelper::RunIp (routers.Get(i), Seconds (2), cmd.str());

        cmd.str(std::string());
        cmd << "route add 10.1.0.0/16 dev sim0";
        LinuxStackHelper::RunIp (routers.Get(i), Seconds (2), cmd.str());

        //std::cout << "NODE " << 1 << ": " << cmd.str() << std::endl;

        /*Connect the routers to the backbone*/
        pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
        pointToPoint.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
        NetDeviceContainer serverDev = pointToPoint.Install(routers.Get(i), serverGw.Get(0));

        routerDevices.Add(serverDev.Get(0));

        cmd.str(std::string());
        cmd << "link set dev sim1 up";
        LinuxStackHelper::RunIp (routers.Get(i), Seconds (1), cmd.str());

        cmd.str(std::string());
        cmd << "addr add 192.167." << i << ".2/24 dev sim1";
        //std::cout << "Router " << i << ": " << cmd.str() << std::endl;
        LinuxStackHelper::RunIp (routers.Get(i), Seconds (2), cmd.str());

        cmd.str(std::string());
        cmd << "route add default via 192.167." << i << ".1 dev sim1";
        LinuxStackHelper::RunIp (routers.Get(i), Seconds (3), cmd.str());

        /*Setup the server gateway links*/

        int sgwDevNumber = serverGw.Get(0)->GetNDevices() - 1;
        cmd.str(std::string());
        cmd << "link set dev sim" << sgwDevNumber << " up";
        LinuxStackHelper::RunIp (serverGw.Get(0), Seconds (1), cmd.str());

        cmd.str(std::string());
        cmd << "addr add 192.167." << i << ".1/24 dev sim" << sgwDevNumber << "";
        LinuxStackHelper::RunIp (serverGw.Get(0), Seconds (3), cmd.str());

        cmd.str(std::string());
        cmd << "route add 192.168." << i + 1 << ".0/24 dev sim" << sgwDevNumber << "";
        LinuxStackHelper::RunIp (serverGw.Get(0), Seconds (3), cmd.str());

    }

    /*****
    * Setup the connection between server gateway and server
    */
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
    NetDeviceContainer serverGwDev = pointToPoint.Install(servers.Get(0), serverGw.Get(0));

    serverDevices.Add(serverGwDev.Get(0));

    std::stringstream mcmd;
    mcmd << "link set dev sim0 up";
    LinuxStackHelper::RunIp (servers.Get(0), Seconds (1), mcmd.str());

    mcmd.str(std::string());
    mcmd << "addr add 194.80.39.1/24 dev sim0";
    LinuxStackHelper::RunIp (servers.Get(0), Seconds (2), mcmd.str());

    mcmd.str(std::string());
    mcmd << "route add default via 194.80.39.2 dev sim0";
    LinuxStackHelper::RunIp (servers.Get(0), Seconds (3), mcmd.str());


    /*****
    * Setup the connection between server gateway
    */

    int sgwDevNumber = serverGw.Get(0)->GetNDevices() - 1;
    mcmd.str(std::string());
    mcmd << "link set dev sim" << sgwDevNumber << " up";
    LinuxStackHelper::RunIp (serverGw.Get(0), Seconds (1), mcmd.str());

    mcmd.str(std::string());
    mcmd << "addr add 194.80.39.2/24 dev sim" << sgwDevNumber << "";
    LinuxStackHelper::RunIp (serverGw.Get(0), Seconds (2), mcmd.str());

    /*****
    * Add load balancing routes if appropriate
    */

    /*Enable load balancing*/

    if (mode == MODE_TCP_LB){

        if(distributeGateways != DIST_GATEWAYS_YES){
            for (int i = 0; i < routers.GetN(); i++){
                int nodeIdx = 0;
                std::stringstream cmd;
                cmd.str(std::string());
                cmd << "route add default scope global";
                for (int i = 0; i < flows; i++){
                    cmd << " nexthop via 192.168." << i << ".1 dev sim" << i << " weight 1";
                }
                cmd << "";
                //std::cout << "LB Gateway: " << cmd.str() << "\n";
                LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
            }
        } else {
            std::stringstream cmd;

            for (int i = 0; i < nodes.GetN(); i++) {
                std::vector< std::string > addresses;

                for (int j = 0; j < routers.GetN(); j++){
                    int nodeIdx = 0;
                    if(distributeGateways == DIST_GATEWAYS_YES){
                        nodeIdx = j % routers.GetN();
                    }
                    if(nodeIdx == i){
                        std::stringstream defrt;
                        int devNumber = nodes.Get(i)->GetNDevices();
                        defrt << "192.168." << j << ".1 dev sim" << devNumber;
                        addresses.push_back(defrt.str());
                    }
                }

                int parent = get_parent(i, treeStride);
                if(parent >= 0){
                    cmd.str(std::string());
                    cmd << "route add default scope global";
                    for (int i = 0; i < flows; i++){
                        cmd << " nexthop via 10.1." << parent + 1 << ".1 dev sim0 weight 1";
                    }
                    cmd << "";
                    //std::cout << "LB Gateway: " << cmd.str() << "\n";
                    LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
                }

                if(addresses.size() > 0) {
                    std::stringstream cmd;
                    cmd.str(std::string());
                    cmd << "route add default scope global";

                    for (int devId = 0; i < addresses.size(); i++) {
                        cmd << " nexthop via " << addresses[i] << " weight 1";
                    }
                    cmd << "";
                    //std::cout << "LB Gateway: " << cmd.str() << "\n";
                    LinuxStackHelper::RunIp (nodes.Get (i), Seconds (0.1), cmd.str());
                }
            }
        }
    }

    /*****
    * Launch Applications
    */

    dce.SetStackSize(1 << 20);
    appHelper.SetStackSize(1 << 20);

    for (int i = 0; i < allHosts.GetN()-2; i++) {
        for (int j = 0; j < allHosts.Get(j)->GetNDevices(); j++){
            std::stringstream nic;
            nic << "sim" << j "";
            start_nat(allHosts.Get(j), nic);
        }
    }

    for (int i = 0; i < allHosts.GetN(); i++) {
        stack.SysctlSet(allHosts.Get(i), ".net.ipv4.conf.all.forwarding", "1");
        stack.SysctlSet(allHosts.Get(i), ".net.ipv6.conf.all.disable_ipv6", "1");
        stack.SysctlSet (allHosts.Get(i), ".net.ipv4.tcp_low_latency", "1");
        stack.SysctlSet (allHosts.Get(i), ".net.ipv4.tcp_no_metrics_save", "1");
        stack.SysctlSet (allHosts.Get(i), ".net.ipv4.tcp_timestamps", "1");
        stack.SysctlSet (allHosts.Get(i), ".net.ipv4.tcp_sack", "1");
        stack.SysctlSet (allHosts.Get(i), ".net.ipv4.tcp_mem", "768174 10242330 15363480");
        stack.SysctlSet (allHosts.Get(i), ".net.ipv4.tcp_rmem", "4096 524288 204217728");
        stack.SysctlSet (allHosts.Get(i), ".net.ipv4.tcp_wmem", "4096 524288 204217728");
        stack.SysctlSet (allHosts.Get(i), ".net.core.wmem_max", "5242870");
        stack.SysctlSet (allHosts.Get(i), ".net.core.rmem_max","5242870");
        stack.SysctlSet (allHosts.Get(i), ".net.core.optmem_max","5242870");
        stack.SysctlSet (allHosts.Get(i), ".net.core.netdev_max_backlog", "25000000");

        LinuxStackHelper::RunIp (allHosts.Get(i), Seconds (4), "route show");
        LinuxStackHelper::RunIp (allHosts.Get(i), Seconds (4), "rule show");
        LinuxStackHelper::RunIp (allHosts.Get(i), Seconds (4), "addr show");
        LinuxStackHelper::RunIp (allHosts.Get (i), Seconds (4), "route show table 1");
        LinuxStackHelper::RunIp (allHosts.Get (i), Seconds (4), "route show table 2");

        /*Enable or disable MPTCP*/
        if (mode == MODE_MPTCP_MPDP || mode == MODE_MPTCP) {
            stack.SysctlSet (allHosts.Get (i), ".net.mptcp.mptcp_enabled", "1");
            stack.SysctlSet (allHosts.Get (i), ".net.mptcp.mptcp_path_manager", "fullmesh");
            stack.SysctlSet(allHosts.Get (i), ".net.ipv4.tcp_congestion_control", ccalg);
        } else {
            stack.SysctlSet (allHosts.Get (i), ".net.mptcp.mptcp_enabled", "0");
            stack.SysctlSet(allHosts.Get (i), ".net.ipv4.tcp_congestion_control", ccalg);
        }

    }

    /****
    *
    * Setup the MPDD
    *
    ****/

    appHelper.SetBinary("iperf");
    appHelper.ResetArguments();
    appHelper.ResetEnvironment();
    appHelper.AddArgument("-s");
    apps = appHelper.Install(servers);
    apps.Start(Seconds (5));
    /*
    if(iperfloc == 1){
        for (int i = 0; i < nodes.GetN(); i++) {
            appHelper.SetBinary("iperf");
            appHelper.ResetArguments();
            appHelper.ResetEnvironment();

            appHelper.AddArgument("-c");
            appHelper.AddArgument("194.80.39.1");
            appHelper.AddArgument("-t");
            appHelper.AddArgument("10");
            appHelper.AddArgument("-i");
            appHelper.AddArgument("2");
            apps = appHelper.InstallInNode(nodes.Get(i));
            std::cout << "Installed in " << i << "\n";
        }
        apps.Start(Seconds (10));
    } else {

        appHelper.SetBinary("iperf");
        appHelper.ResetArguments();
        appHelper.ResetEnvironment();
        appHelper.AddArgument("-c");
        appHelper.AddArgument("194.80.39.1");
        appHelper.AddArgument("-t");
        appHelper.AddArgument("10");
        appHelper.AddArgument("-i");
        appHelper.AddArgument("2");
        apps = appHelper.InstallInNode(nodes.Get(nDevices-1));
        apps.Start(Seconds (10));

    }
    */
    LinuxStackHelper::SysctlGet (nodes.Get (nDevices-1), Seconds (5),".net.mptcp.mptcp_enabled", &PrintTcpFlags);
    LinuxStackHelper::SysctlGet (nodes.Get (nDevices-1), Seconds (5),".net.mptcp.mptcp_path_manager", &PrintTcpFlags);
    LinuxStackHelper::SysctlGet (nodes.Get (nDevices-1), Seconds (5),".net.ipv4.tcp_congestion_control", &PrintTcpFlags);

    appHelper.SetBinary("ping");
    appHelper.ResetArguments();
    appHelper.ResetEnvironment();
    appHelper.AddArgument("194.80.39.2");
    appHelper.AddArgument("-I");
    appHelper.AddArgument("192.168.7.1");
    apps = appHelper.InstallInNode(nodes.Get(nDevices-1));
    apps.Start(Seconds (6));

    appHelper.SetBinary("ping");
    appHelper.ResetArguments();
    appHelper.ResetEnvironment();
    appHelper.AddArgument("194.80.39.2");
    appHelper.AddArgument("-I");
    appHelper.AddArgument("10.1.3.7");
    apps = appHelper.InstallInNode(nodes.Get(nDevices-1));
    apps.Start(Seconds (6));

    /*Enable the MPDP*/

    if(mode == MODE_MPTCP_MPDP || mode == MODE_TCP_MPDP_LB){
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

    }

    std::cout << "DONE\n";

    //csma.EnablePcap("dce-mpdd-nested-csma-ap", apDevices, true);
    csma.EnablePcap("dce-mpdd-nested-csma-sta", staDevices, true);

    pointToPoint.EnablePcap("dce-mpdd-nested-ptp-routers", routerDevices, true);
    pointToPoint.EnablePcap("dce-mpdd-nested-ptp-servers", serverDevices, true);

    Simulator::Stop(Seconds(60));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
