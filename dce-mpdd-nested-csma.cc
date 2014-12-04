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

uint32_t
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

void cantor(Ptr<ListPositionAllocator> positionAlloc, float x, float y, float len, float maxHeight) {

    if(maxHeight > 0) maxHeight--;
    else return;

    //line(x,y,x+len,y);
    positionAlloc->Add (ns3::Vector (x+(len/2), y, 0.0));

    y += 10.00;


    cantor(positionAlloc, x, y, len/3, maxHeight);
    cantor(positionAlloc, x+len*2/3, y, len/3, maxHeight);
}

int main(int argc, char *argv[])
{
    std::string wifiBaseName = "WiFi";

    uint32_t nDevices = 15;

    float initX = 0.00;
    float initY = 0.00;
    float deltaY = 10.00;
    float deltaX = 10.00;
    uint32_t treeStride = 2;
    uint32_t nInterfaces = 1;
    uint32_t nRootInterfaces = 0;
    uint32_t leaves = 0;
    uint32_t height = 0;

    float posX = initX;
    float posY = initY;

    CommandLine cmd;
    cmd.AddValue("devices", "Number of wifi devices", nDevices);
    cmd.AddValue("stride", "Tree Stride", treeStride);
    cmd.AddValue("interfaces", "Number of gateway interfaces for non root devices", nInterfaces);
    cmd.AddValue("root_interfaces", "Number of gateway interfaces for root device", nRootInterfaces);
    cmd.AddValue("spacing", "Space between devices", deltaX);

    deltaY = deltaX;

    cmd.Parse(argc, argv);

    height = get_height(treeStride, nDevices);
    leaves = get_leaves(treeStride, height);

    std::cout << "Tree Height: " << height << " Tree Leaves: " << leaves << std::endl;

    NodeContainer nodes;

    nodes.Create(nDevices);

    InternetStackHelper internet;
    internet.Install(nodes);


    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;

    for (long i = 0; i < nodes.GetN(); i++) {
        NodeContainer tempNodes;
        CsmaHelper csma;
        csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
        csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (10000000)));

        uint32_t firstChild = get_first_child(i, treeStride);

        //NetDeviceContainer dev = csma.Install(nodes.Get(i));
        tempNodes.Add(nodes.Get(i));

        for(int j = 0; j < treeStride; j++){
            if((firstChild+j) >= nodes.GetN()) break;
            //dev = csma.Install(nodes.Get(firstChild+j));
            //staDevices.Add(dev);
            tempNodes.Add(nodes.Get(firstChild+j));
        }

        NetDeviceContainer devices = csma.Install(tempNodes);
        apDevices.Add(devices.Get(0));
        for(int j = 1; j <= treeStride; j++){
            if((firstChild+j) >= nodes.GetN()) break;
            staDevices.Add(devices.Get(j));
        }
    }

    for (long i = 0; i < nodes.GetN(); i++) {
        std::stringstream tempAddress;
        Ipv4AddressHelper address;
        NetDeviceContainer apDev = apDevices.Get(i);
        tempAddress << "192.168." << i << ".0";
        address.SetBase(tempAddress.str().c_str(), "255.255.255.0");
        address.Assign(apDev);

        uint32_t firstChild = get_first_child(i, treeStride);

        for(int j = 0; j < treeStride; j++){
            if((firstChild+j-1) >= staDevices.GetN()) break;
            NetDeviceContainer staDev = staDevices.Get(firstChild+j-1);
            address.Assign(staDev);
        }
    }





    Simulator::Stop(Seconds(15.05));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
