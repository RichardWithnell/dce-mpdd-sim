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

    uint32_t nDevices = 1;

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
    MobilityHelper mobility;
    WifiHelper wifi = WifiHelper::Default();
    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();

    InternetStackHelper internet;
    internet.Install(nodes);
    std::vector< Ptr<YansWifiChannel> > channels;
    std::vector< Ipv4AddressHelper > addresses;
    /*
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(10.0),
        "DeltaY", DoubleValue(10.0),
        "GridWidth", UintegerValue(5),
        "LayoutType", StringValue("ColumnFirst"));
    */
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;
    int j = 1;
    long expWidth = deltaX * leaves;
    std::cout << "ExpWidth: " << expWidth << std::endl;

    //cantor(positionAlloc, 0, 0, expWidth, height+1);

    for (long i = 0; i < nodes.GetN(); i++) {
        std::stringstream tempWifiName, tempAddress;
        tempWifiName << "" << wifiBaseName << "-" << i;
        Ssid apSsid = Ssid(tempWifiName.str());
        tempAddress << "192.168." << i << ".0";
        //address.SetBase(tempAddress.str().c_str(), "255.255.255.0");


        //std::cout << tempWifiName.str() << std::endl;
        //std::cout << "" << tempAddress.str().c_str() << std::endl;

        Ptr<YansWifiChannel> chan = wifiChannel.Create();

        wifiPhy.SetChannel(chan);
        channels.push_back(chan);
        //addresses.push_back(address);
        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(apSsid));
        NetDeviceContainer dev = wifi.Install(wifiPhy, wifiMac, nodes.Get(i));
        apDevices.Add(dev);
        //address.Assign(dev);
        std::cout << "Equals: " << i+1 << " : " << (long)pow(treeStride, j) << std::endl;
        long nNodes = (long)pow(treeStride, j);
        if(i == 0){
            std::cout << "Coord( " << posX << " , " << posY << " )" << std::endl;
            positionAlloc->Add (ns3::Vector (posX, posY, 0.0));
            posY += deltaY;
        } else if(nNodes == i){
            //std::cout << "Increment Y " << nNodes << std::endl;
            deltaX = expWidth/nNodes;
            posX = initX-(expWidth/2)+(deltaX/nNodes);
            //posX = initX;
            std::cout << "Coord( " << posX << " , " << posY << " )" << std::endl;
            positionAlloc->Add (ns3::Vector (posX, posY, 0.0));
            posY += deltaY;
            j++;
        } else {
            posX += deltaX;
            std::cout << "Coord( " << posX << " , " << posY << " )" << std::endl;
            positionAlloc->Add (ns3::Vector (posX, posY, 0.0));
        }

        //std::cout << "Coord: " << posX << " , " << posY << std::endl;

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

    //mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue (Rectangle (-1000, 1000, -1000, 1000)));

    mobility.SetPositionAllocator (positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);


    /*

    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    */
    Simulator::Stop(Seconds(15.05));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
