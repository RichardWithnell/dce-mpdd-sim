// Adapted from NS3 examples/dce-mptcp-lte-wifi.cc
//

// http://inl.info.ucl.ac.be/system/files/phd-thesis_1.pdf
// http://people.cs.umass.edu/~yungchih/publication/12_mtcp_4g_tech_report.pdf

#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/dce-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/config-store-module.h"

using namespace ns3;

#define MODE_TCP 0
#define MODE_MPTCP_FM 1
#define MODE_MPTCP_ND 2
#define MODE_TCP_LB 3

Ptr<NormalRandomVariable> createNormalRandomVariable(double mean, double variance, double bound){
    Ptr<NormalRandomVariable> x = CreateObject<NormalRandomVariable> ();
    x->SetAttribute ("Mean", DoubleValue (mean));
    //x->SetAttribute ("Variance", DoubleValue (variance));
    x->SetAttribute ("Bound", DoubleValue (bound));
    return x;
}

/**
Three HSPA
MIN: 323.140
MAX: 1510.172
Average: 423.240
Deviation: 224.801
**/

#define DEVICE_ONE_MEAN_RTT (423.240/2.00)
#define DEVICE_ONE_VARIANCE_RTT ((224.801/2.00)*(224.801/2.00))
#define DEVICE_ONE_BOUNDS_RTT (224.801/2.00)


/**
Three HSPA +
Min: 43.600
Max: 138.000
Average: 61.939
Deviation: 14.836
**/

#define DEVICE_TWO_MEAN_RTT (61.939/2.00)
#define DEVICE_TWO_BOUNDS_RTT (14.836/2.00)
#define DEVICE_TWO_VARIANCE_RTT ((14.836/2.00)*(14.836/2.00))

#define DEVICE_THREE_MEAN_RTT 100
#define DEVICE_THREE_VARIANCE_RTT 40
#define DEVICE_THREE_BOUNDS_RTT 20

#define DEVICE_FOUR_MEAN_RTT 100
#define DEVICE_FOUR_VARIANCE_RTT 40
#define DEVICE_FOUR_BOUNDS_RTT 20

bool netDevCbFour(
  Ptr<NetDevice> device,
  Ptr<const Packet> pkt,
  uint16_t  protocol,
  const Address &from)
{
    static Ptr<NormalRandomVariable> x =
        createNormalRandomVariable(DEVICE_ONE_MEAN_RTT,
                                   DEVICE_ONE_VARIANCE_RTT,
                                   DEVICE_ONE_BOUNDS_RTT);
    Ptr<Node> node = device->GetNode ();
    Simulator::Schedule(MilliSeconds(x->GetValue()), &Node::NonPromiscReceiveFromDevice, node, device, pkt, protocol, from);
    return  true;
}


bool netDevCbThree(
  Ptr<NetDevice> device,
  Ptr<const Packet> pkt,
  uint16_t  protocol,
const Address &from)
{
    static Ptr<NormalRandomVariable> x =
        createNormalRandomVariable(DEVICE_TWO_MEAN_RTT,
                                   DEVICE_TWO_VARIANCE_RTT,
                                   DEVICE_TWO_BOUNDS_RTT);
    Ptr<Node> node = device->GetNode ();
    Simulator::Schedule(MilliSeconds(x->GetValue()), &Node::NonPromiscReceiveFromDevice, node, device, pkt, protocol, from);
    return  true;
}


bool netDevCbTwo(
  Ptr<NetDevice> device,
  Ptr<const Packet> pkt,
  uint16_t  protocol,
const Address &from)
{
    static Ptr<NormalRandomVariable> x =
        createNormalRandomVariable(DEVICE_THREE_MEAN_RTT,
                                   DEVICE_THREE_VARIANCE_RTT,
                                   DEVICE_THREE_BOUNDS_RTT);
    Ptr<Node> node = device->GetNode ();
    Simulator::Schedule(MilliSeconds(x->GetValue()), &Node::NonPromiscReceiveFromDevice, node, device, pkt, protocol, from);
    return  true;
}


bool netDevCbOne(
  Ptr<NetDevice> device,
  Ptr<const Packet> pkt,
  uint16_t  protocol,
  const Address &from)
{
    static Ptr<NormalRandomVariable> x =
        createNormalRandomVariable(DEVICE_FOUR_MEAN_RTT,
                                   DEVICE_FOUR_VARIANCE_RTT,
                                   DEVICE_FOUR_BOUNDS_RTT);
    Ptr<Node> node = device->GetNode ();
    Simulator::Schedule(MilliSeconds(x->GetValue()), &Node::NonPromiscReceiveFromDevice, node, device, pkt, protocol, from);
    return  true;
}


void setPos (Ptr<Node> n, int x, int y, int z)
{
    Ptr<ConstantPositionMobilityModel> loc = CreateObject<ConstantPositionMobilityModel> ();
    n->AggregateObject (loc);
    Vector locVec2 (x, y, z);
    loc->SetPosition (locVec2);
}

void
PrintTcpFlags (std::string key, std::string value)
{
    std::cout << key << "=" << value;
}

int main (int argc, char *argv[])
{
    double stopTime = 80.0;
    std::string p2pdelay = "50ms";
    std::string iperfTime = "60";
    std::string ccalg = "reno";
    int flows = 1;
    int mode = MODE_TCP;
    int debug = 0;
    int delay = 0;

    Callback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address&> callbacks[4];

    CommandLine cmd;
    cmd.AddValue ("stopTime", "StopTime of simulatino.", stopTime);
    cmd.AddValue ("p2pDelay", "Delay of p2p links. default is 50ms.", p2pdelay);
    cmd.AddValue ("flows", "Number of TCP flows. Default is 1", flows);
    cmd.AddValue ("mode", "TCP (0), MPTCP full mesh (1) or MPTCP ndifforts(2). Default is MPTCP full mesh.", mode);
    cmd.AddValue ("debug", "Turn MPTCP debug on or off", debug);
    cmd.AddValue ("ccalg", "Set TCP Congestion Control Algorithm.", ccalg);
    cmd.AddValue ("delay", "Set variable delay on or off", delay);
    cmd.Parse (argc, argv);

    PointToPointHelper pointToPointServer;
    PointToPointHelper pointToPointClient;
    NetDeviceContainer devices1, devices2, serverDevices;

    NodeContainer nodes;
    LinuxStackHelper stack;
    DceManagerHelper dceManager;

    NetDeviceContainer clientDevices;

    if(delay){
        callbacks[0] = MakeCallback(&netDevCbOne);
        callbacks[1] = MakeCallback(&netDevCbTwo);
        callbacks[2] = MakeCallback(&netDevCbThree);
        callbacks[3] = MakeCallback(&netDevCbFour);
    }

    GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));
    nodes.Create (3);

    dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux.so"));

    stack.Install (nodes);
    dceManager.Install (nodes);

    std::cout << "Subflows: " << flows << " Mode: " << mode << "\n";

    pointToPointServer.SetDeviceAttribute ("DataRate", StringValue ("1Gb/s"));
    pointToPointServer.SetChannelAttribute ("Delay", StringValue ("0ms"));
    serverDevices = pointToPointServer.Install(nodes.Get(1), nodes.Get(2));

    /*Setup Server Routes*/
    LinuxStackHelper::RunIp (nodes.Get (2), Seconds (0.1), "link set up dev sim0");
    LinuxStackHelper::RunIp (nodes.Get (2), Seconds (0.1), "addr add 172.16.1.1/24 dev sim0");
    LinuxStackHelper::RunIp (nodes.Get (2), Seconds (0.1), "route add 192.168.0.0/16 via 172.16.1.10 dev sim0");

    /*Setup Gateway->Server*/
    LinuxStackHelper::RunIp (nodes.Get (1), Seconds (0.1), "link set up dev sim0");
    LinuxStackHelper::RunIp (nodes.Get (1), Seconds (0.1), "addr add 172.16.1.10/24 dev sim0");


    pointToPointClient.SetDeviceAttribute ("DataRate", StringValue ("10Mb/s"));
    pointToPointClient.SetChannelAttribute ("Delay", StringValue (p2pdelay));

    setPos (nodes.Get (0), 50, 15, 0);
    setPos (nodes.Get (1), 100, 15, 0);
    setPos (nodes.Get (2), 150, 15, 0);

    for (int i = 0; i < flows; i++){
        NetDeviceContainer netDevContainer = pointToPointClient.Install(nodes.Get(0), nodes.Get(1));
        Ptr<NetDevice> clientDevice = netDevContainer.Get(0);
        Ptr<NetDevice> gatewayDevice = netDevContainer.Get(1);
        if(delay){
            int whichDelay = i % 4;
            clientDevice->SetReceiveCallback(callbacks[whichDelay]);
            gatewayDevice->SetReceiveCallback(callbacks[whichDelay]);
        }

        clientDevices.Add(clientDevice);

        std::stringstream cmd;

        /*Setup Client Addresses and routes*/

        cmd << "link set up dev sim" << i;
        LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
        cmd.str(std::string());

        cmd << "addr add 192.168." << i << ".10/24 dev sim" << i;
        LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
        cmd.str(std::string());

        if(mode != MODE_TCP_LB){
            cmd << "route add default via 192.168." << i << ".1 dev sim" << i << " metric " << i + 1;
            LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
            cmd.str(std::string());
        }

        cmd << "rule add from 192.168." << i << ".0/24 lookup " << i + 1;
        LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
        cmd.str(std::string());


        cmd << "route add default via 192.168." << i << ".1 dev sim" << i << " table " << i + 1;
        LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
        cmd.str(std::string());

        cmd << "route add 192.168." << i << ".0/24 dev sim" << i << " table " << i + 1;
        LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
        cmd.str(std::string());


        //cmd << "rule add fwmark " << i + 1 << " lookup " << i + 1;
        //LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
        //cmd.str(std::string());

        /*Setup Gateway Addresses*/

        cmd << "link set up dev sim" << i + 1;
        LinuxStackHelper::RunIp (nodes.Get (1), Seconds (0.1), cmd.str());
        cmd.str(std::string());

        cmd << "addr add 192.168." << i << ".1/24 dev sim" << i + 1;
        LinuxStackHelper::RunIp (nodes.Get (1), Seconds (0.1), cmd.str());
        cmd.str(std::string());

    }
    if (mode == MODE_TCP_LB){
        std::stringstream cmd;
        cmd.str(std::string());
        cmd << "route add default scope global";
        for (int i = 0; i < flows; i++){
            cmd << " nexthop via 192.168." << i << ".1 dev sim" << i << " weight 1";
        }
        cmd << "";
        std::cout << "LB Gateway: " << cmd.str() << "\n";
        LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
    }

    LinuxStackHelper::RunIp (nodes.Get (0), Seconds (1), "addr show");
    LinuxStackHelper::RunIp (nodes.Get (0), Seconds (1), "rule show");
    LinuxStackHelper::RunIp (nodes.Get (0), Seconds (1), "route show");

    LinuxStackHelper::RunIp (nodes.Get (1), Seconds (1), "addr show");
    LinuxStackHelper::RunIp (nodes.Get (1), Seconds (1), "rule show");
    LinuxStackHelper::RunIp (nodes.Get (1), Seconds (1), "route show");

    LinuxStackHelper::RunIp (nodes.Get (2), Seconds (1), "addr show");
    LinuxStackHelper::RunIp (nodes.Get (2), Seconds (1), "rule show");
    LinuxStackHelper::RunIp (nodes.Get (2), Seconds (1), "route show");

    stack.SysctlSet (nodes, ".net.ipv4.conf.default.forwarding", "1");
    stack.SysctlSet (nodes.Get (0), ".net.ipv4.tcp_low_latency", "1");
    stack.SysctlSet (nodes.Get (0), ".net.ipv4.tcp_no_metrics_save", "1");
    stack.SysctlSet (nodes.Get (0), ".net.ipv4.tcp_timestamps", "1");
    stack.SysctlSet (nodes.Get (0), ".net.ipv4.tcp_sack", "1");
    stack.SysctlSet (nodes.Get (0), ".net.ipv4.tcp_mem", "768174 10242330 15363480");
    stack.SysctlSet (nodes.Get (0), ".net.ipv4.tcp_rmem", "4096 524288 204217728");
    stack.SysctlSet (nodes.Get (0), ".net.ipv4.tcp_wmem", "4096 524288 204217728");
    stack.SysctlSet (nodes.Get (0), ".net.core.wmem_max", "5242870");
    stack.SysctlSet (nodes.Get (0), ".net.core.rmem_max","5242870");
    stack.SysctlSet (nodes.Get (0), ".net.core.optmem_max","5242870");
    stack.SysctlSet (nodes.Get (0), ".net.core.netdev_max_backlog", "25000000");

    stack.SysctlSet (nodes.Get (1), ".net.ipv4.tcp_low_latency", "1");
    stack.SysctlSet (nodes.Get (1), ".net.ipv4.tcp_no_metrics_save", "1");
    stack.SysctlSet (nodes.Get (1), ".net.ipv4.tcp_timestamps", "1");
    stack.SysctlSet (nodes.Get (1), ".net.ipv4.tcp_sack", "1");
    stack.SysctlSet (nodes.Get (1), ".net.ipv4.tcp_mem", "768174 10242330 15363480");
    stack.SysctlSet (nodes.Get (1), ".net.ipv4.tcp_rmem", "4096 524288 204217728");
    stack.SysctlSet (nodes.Get (1), ".net.ipv4.tcp_wmem", "4096 524288 204217728");
    stack.SysctlSet (nodes.Get (1), ".net.core.wmem_max", "5242870");
    stack.SysctlSet (nodes.Get (1), ".net.core.rmem_max","5242870");
    stack.SysctlSet (nodes.Get (1), ".net.core.optmem_max","5242870");
    stack.SysctlSet (nodes.Get (1), ".net.core.netdev_max_backlog", "25000000");

    stack.SysctlSet (nodes.Get (2), ".net.ipv4.tcp_low_latency", "1");
    stack.SysctlSet (nodes.Get (2), ".net.ipv4.tcp_no_metrics_save", "1");
    stack.SysctlSet (nodes.Get (2), ".net.ipv4.tcp_timestamps", "1");
    stack.SysctlSet (nodes.Get (2), ".net.ipv4.tcp_sack", "1");
    stack.SysctlSet (nodes.Get (2), ".net.ipv4.tcp_mem", "768174 10242330 15363480");
    stack.SysctlSet (nodes.Get (2), ".net.ipv4.tcp_rmem", "4096 524288 204217728");
    stack.SysctlSet (nodes.Get (2), ".net.ipv4.tcp_wmem", "4096 524288 204217728");
    stack.SysctlSet (nodes.Get (2), ".net.core.wmem_max", "5242870");
    stack.SysctlSet (nodes.Get (2), ".net.core.rmem_max","5242870");
    stack.SysctlSet (nodes.Get (2), ".net.core.optmem_max","5242870");
    stack.SysctlSet (nodes.Get (2), ".net.core.netdev_max_backlog", "25000000");

    if(debug){
        stack.SysctlSet (nodes, ".net.mptcp.mptcp_debug", "1");
    } else {
        stack.SysctlSet (nodes, ".net.mptcp.mptcp_debug", "0");
    }

    if (mode == MODE_MPTCP_FM) {
        stack.SysctlSet (nodes, ".net.mptcp.mptcp_enabled", "1");
        stack.SysctlSet (nodes, ".net.mptcp.mptcp_path_manager", "fullmesh");
        stack.SysctlSet(nodes, ".net.ipv4.tcp_congestion_control", ccalg);
    } else if (mode == MODE_MPTCP_ND) {
        stack.SysctlSet (nodes, ".net.mptcp.mptcp_enabled", "1");
        stack.SysctlSet (nodes, ".net.mptcp.mptcp_path_manager", "ndiffports");
        stack.SysctlSet(nodes, ".net.ipv4.tcp_congestion_control", ccalg);
    } else {
        stack.SysctlSet (nodes, ".net.mptcp.mptcp_enabled", "0");
        stack.SysctlSet(nodes, ".net.ipv4.tcp_congestion_control", ccalg);
    }


    LinuxStackHelper::SysctlGet (nodes.Get (0), Seconds (1),".net.ipv4.tcp_available_congestion_control", &PrintTcpFlags);
    LinuxStackHelper::SysctlGet (nodes.Get (0), Seconds (1),".net.ipv4.tcp_rmem", &PrintTcpFlags);
    LinuxStackHelper::SysctlGet (nodes.Get (0), Seconds (1),".net.ipv4.tcp_wmem", &PrintTcpFlags);
    LinuxStackHelper::SysctlGet (nodes.Get (0), Seconds (1),".net.ipv4.tcp_mem", &PrintTcpFlags);
    LinuxStackHelper::SysctlGet (nodes.Get (0), Seconds (1),".net.core.rmem_max", &PrintTcpFlags);
    LinuxStackHelper::SysctlGet (nodes.Get (0), Seconds (1),".net.core.wmem_max", &PrintTcpFlags);
    LinuxStackHelper::SysctlGet (nodes.Get (0), Seconds (1),".net.core.optmem_max", &PrintTcpFlags);
    LinuxStackHelper::SysctlGet (nodes.Get (0), Seconds (1),".net.core.netdev_max_backlog", &PrintTcpFlags);

    LinuxStackHelper::SysctlGet (nodes.Get (0), Seconds (1),".net.mptcp.mptcp_enabled", &PrintTcpFlags);
    LinuxStackHelper::SysctlGet (nodes.Get (0), Seconds (1),".net.mptcp.mptcp_path_manager", &PrintTcpFlags);
    LinuxStackHelper::SysctlGet (nodes.Get (0), Seconds (1),".net.ipv4.tcp_congestion_control", &PrintTcpFlags);



    DceApplicationHelper dce;
    ApplicationContainer apps;

    dce.SetStackSize (1 << 20);

    // Launch iperf client on node 0
    if (mode == MODE_MPTCP_FM || mode == MODE_MPTCP_ND) {
        dce.SetBinary ("iperf");
        dce.ResetArguments ();
        dce.ResetEnvironment ();
        dce.AddArgument ("-c");
        dce.AddArgument ("172.16.1.1");
        //dce.ParseArguments ("-y C");
        dce.AddArgument ("-i");
        dce.AddArgument ("1");
        dce.AddArgument ("--time");
        dce.AddArgument (iperfTime);
        dce.AddArgument ("-m");

        apps = dce.Install (nodes.Get (0));
        apps.Start (Seconds (10.0));
        std::cout << "iperf -c 172.16.1.1 -i 1 --time 60\n";
    } else if(mode == MODE_TCP) {
        for(int i = 0; i < flows; i++){
            std::stringstream bind;

            bind << "192.168." << i << ".10";

            dce.SetBinary ("iperf");
            dce.ResetArguments ();
            dce.ResetEnvironment ();
            dce.AddArgument("-B");
            dce.AddArgument(bind.str());
            dce.AddArgument ("-c");
            dce.AddArgument ("172.16.1.1");
            //dce.ParseArguments ("-y C");
            dce.AddArgument ("-i");
            dce.AddArgument ("1");
            dce.AddArgument ("--time");
            dce.AddArgument (iperfTime);
            dce.AddArgument ("-m");

            apps = dce.Install (nodes.Get (0));
            apps.Start (Seconds (10.0));
            std::cout << "iperf -c 172.16.1.1 -i 1 --time 60 -B" << bind.str() << "\n";
        }
    } else {
        std::stringstream flowstr;

        flowstr << "" << flows << "";

        dce.SetBinary ("iperf");
        dce.ResetArguments ();
        dce.ResetEnvironment ();
        dce.AddArgument ("-c");
        dce.AddArgument ("172.16.1.1");
        dce.AddArgument("-P");
        dce.AddArgument(flowstr.str());
        //dce.ParseArguments ("-y C");
        dce.AddArgument ("--time");
        dce.AddArgument (iperfTime);

        apps = dce.Install (nodes.Get (0));
        apps.Start (Seconds (10.0));
        std::cout << "iperf -c 172.16.1.1 -i 1 --time 60 -P" << flowstr.str() << "\n";
    }

    dce.SetBinary ("iperf");
    dce.ResetArguments ();
    dce.ResetEnvironment ();
    dce.AddArgument ("-s");

    apps = dce.Install (nodes.Get (2));
    apps.Start (Seconds (8.00));

    pointToPointClient.EnablePcap("mptcp-subflows", clientDevices, false);

    LinuxStackHelper::PopulateRoutingTables ();

    // Output config store to txt format
    Config::SetDefault ("ns3::ConfigStore::Filename", StringValue ("output-attributes.txt"));
    Config::SetDefault ("ns3::ConfigStore::FileFormat", StringValue ("RawText"));
    Config::SetDefault ("ns3::ConfigStore::Mode", StringValue ("Save"));
    ConfigStore outputConfig2;
    outputConfig2.ConfigureDefaults ();
    outputConfig2.ConfigureAttributes ();

    Simulator::Stop (Seconds (stopTime));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}
