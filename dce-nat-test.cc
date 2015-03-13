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

Ptr<NormalRandomVariable> createNormalRandomVariable(double mean, double variance, double bound){
    Ptr<NormalRandomVariable> x = CreateObject<NormalRandomVariable> ();
    x->SetAttribute ("Mean", DoubleValue (mean));
    //x->SetAttribute ("Variance", DoubleValue (variance));
    x->SetAttribute ("Bound", DoubleValue (bound));
    return x;
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
    double stopTime = 20.0;
    std::string p2pdelay = "50ms";
    std::string iperfTime = "60";
    std::string ccalg = "reno";
    int flows = 1;
    int mode = MODE_TCP;
    int debug = 0;
    int delay = 0;


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

    GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));
    nodes.Create (3);

    dceManager.SetTaskManagerAttribute ("FiberManagerType", StringValue ("UcontextFiberManager"));
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
    //LinuxStackHelper::RunIp (nodes.Get (2), Seconds (0.1), "route add 192.168.0.0/16 via 172.16.1.10 dev sim0");

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
        clientDevices.Add(clientDevice);

        std::stringstream cmd;

        /*Setup Client Addresses and routes*/

        cmd << "link set up dev sim" << i;
        LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
        cmd.str(std::string());

        cmd << "addr add 192.168." << i << ".10/24 dev sim" << i;
        LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
        cmd.str(std::string());

        cmd << "route add default via 192.168." << i << ".1 dev sim" << i << " metric " << i + 1;
        LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
        cmd.str(std::string());

        cmd << "rule add from 192.168." << i << ".0/24 lookup " << i + 1;
        LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
        cmd.str(std::string());

        cmd << "route add default via 192.168." << i << ".1 dev sim" << i << " table " << i + 1;
        LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
        cmd.str(std::string());

        /*
        cmd << "rule add fwmark " << i + 1 << " lookup " << i + 1;
        LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd.str());
        cmd.str(std::string());
        */
        /*Setup Gateway Addresses*/

        cmd << "link set up dev sim" << i + 1;
        LinuxStackHelper::RunIp (nodes.Get (1), Seconds (0.1), cmd.str());
        cmd.str(std::string());

        cmd << "addr add 192.168." << i << ".1/24 dev sim" << i + 1;
        LinuxStackHelper::RunIp (nodes.Get (1), Seconds (0.1), cmd.str());
        cmd.str(std::string());

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


    if(debug){
        stack.SysctlSet (nodes, ".net.mptcp.mptcp_debug", "1");
    } else {
        stack.SysctlSet (nodes, ".net.mptcp.mptcp_debug", "0");
    }


    DceApplicationHelper dce;
    ApplicationContainer apps;

    dce.SetStackSize (1 << 20);

    dce.SetBinary ("xtables-multi");
    dce.ResetArguments ();
    dce.ResetEnvironment ();
    dce.AddArgument ("iptables");
    dce.AddArgument ("-t");
    dce.AddArgument ("nat");
    dce.AddArgument ("-A");
    dce.AddArgument ("POSTROUTING");
    dce.AddArgument ("-o");
    dce.AddArgument ("sim0");
    dce.AddArgument ("-j");
    dce.AddArgument ("MASQUERADE");

    apps = dce.Install(nodes.Get(1));
    apps.Start(Seconds (2.0));

    dce.SetBinary ("xtables-multi");
    dce.ResetArguments ();
    dce.ResetEnvironment ();
    dce.AddArgument ("iptables");
    dce.AddArgument ("-t");
    dce.AddArgument ("nat");
    dce.AddArgument ("-L");

    apps = dce.Install(nodes.Get(1));
    apps.Start(Seconds (2.0));

    // Launch iperf client on node 0
    dce.SetBinary ("ping");
    dce.ResetArguments ();
    dce.ResetEnvironment ();
    dce.AddArgument ("172.16.1.1");

    apps = dce.Install(nodes.Get(0));
    apps.Start(Seconds (5.0));
    //dce.ParseArguments ("-y C");

    pointToPointClient.EnablePcap("dce-nat", clientDevices, false);

    //LinuxStackHelper::PopulateRoutingTables ();

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
