#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"

using namespace ns3;
int main() {
  NodeContainer wifiApNode; wifiApNode.Create(1);
  SpectrumWifiPhyHelper spectrumPhy(2);
  WifiHelper wifi; wifi.SetStandard(WIFI_STANDARD_80211be);
  WifiMacHelper mac; mac.SetType("ns3::ApWifiMac");
  NetDeviceContainer apDevice = wifi.Install(spectrumPhy, mac, wifiApNode);

  BasicEnergySourceHelper basicSourceHelper;
  EnergySourceContainer sources = basicSourceHelper.Install(wifiApNode);
  WifiRadioEnergyModelHelper radioEnergyHelper;
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(apDevice, sources);
  return 0;
}
