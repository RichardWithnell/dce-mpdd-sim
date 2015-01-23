#include "ns3/dce-module.h"
#include "ns3/core-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    DceApplicationHelper dce;
    DceManagerHelper dceManager;
    ApplicationContainer apps;
    LinuxStackHelper stack;

    NodeContainer nodes;

    nodes.Create(2);

    dceManager.SetTaskManagerAttribute ("FiberManagerType", StringValue ("UcontextFiberManager"));
    dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux.so"));

    stack.Install (nodes);

    dceManager.Install (nodes);

    dce.SetStackSize (1 << 20);
    dce.SetBinary ("test_libmnl");
    dce.ResetArguments ();
    dce.ResetEnvironment ();
    apps = dce.Install(nodes);
    apps.Start (Seconds (5));


    Simulator::Stop (Seconds (30.00));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}
