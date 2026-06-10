#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue("5000p"));
  Config::SetDefault("ns3::WifiMacQueue::MaxDelay", TimeValue(Seconds(1.0)));
  
  // A-MPDU Aggregation
  Config::SetDefault("ns3::WifiMac::MpduBufferSize", UintegerValue(1024));
  Config::SetDefault("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue(1048575));
  Config::SetDefault("ns3::WifiMac::BK_MaxAmpduSize", UintegerValue(1048575));
  Config::SetDefault("ns3::WifiMac::VI_MaxAmpduSize", UintegerValue(1048575));
  Config::SetDefault("ns3::WifiMac::VO_MaxAmpduSize", UintegerValue(1048575));

  NodeContainer wifiApNode; wifiApNode.Create(1);
  NodeContainer wifiStaNode; wifiStaNode.Create(1);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(10.0, 10.0, 3.0));
  positionAlloc->Add(Vector(15.0, 10.0, 1.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);
  mobility.Install(wifiStaNode);

  SpectrumWifiPhyHelper spectrumPhy(1); // Non-MLO
  spectrumPhy.SetErrorRateModel("ns3::NistErrorRateModel");
  spectrumPhy.Set("Antennas", UintegerValue(8)); // 8x8 MIMO
  spectrumPhy.Set("MaxSupportedTxSpatialStreams", UintegerValue(8));
  spectrumPhy.Set("MaxSupportedRxSpatialStreams", UintegerValue(8));
  spectrumPhy.Set("TxPowerStart", DoubleValue(20.0));
  spectrumPhy.Set("TxPowerEnd", DoubleValue(20.0));

  Ptr<MultiModelSpectrumChannel> channel5Ghz = CreateObject<MultiModelSpectrumChannel>();
  Ptr<LogDistancePropagationLossModel> loss5 = CreateObject<LogDistancePropagationLossModel>();
  channel5Ghz->AddPropagationLossModel(loss5);
  channel5Ghz->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
  spectrumPhy.Set(0, "ChannelSettings", StringValue("{0, 160, BAND_5GHZ, 0}"));
  spectrumPhy.SetChannel(channel5Ghz);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ax); // Wi-Fi 6
  wifi.SetRemoteStationManager("ns3::IdealWifiManager"); 

  WifiMacHelper mac;
  Ssid ssid = Ssid("Pameran-WiFi6");
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(true));
  NetDeviceContainer apDevice = wifi.Install(spectrumPhy, mac, wifiApNode);
  
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(true));
  NetDeviceContainer staDevice = wifi.Install(spectrumPhy, mac, wifiStaNode);

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

  uint16_t port = 50000;
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(wifiStaNode.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(5.0));

  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(staInterface.GetAddress(0), port));
  // Injeksi di batas kapasitas Wi-Fi 6 agar antrean tidak bocor parah
  onoff.SetAttribute("DataRate", StringValue("3.2Gbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(1472));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer sourceApps = onoff.Install(wifiApNode.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(5.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(5.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (auto const& stat : stats) {
      if (stat.first == 1) {
          // Perhitungan receiver-based duration untuk akurasi rxBytes throughput
          double duration = stat.second.timeLastRxPacket.GetSeconds() - stat.second.timeFirstRxPacket.GetSeconds();
          if (duration <= 0) duration = 4.0;
          
          double throughput = (stat.second.rxBytes * 8.0) / (duration * 1e6);
          double delay = stat.second.rxPackets > 0 ? (stat.second.delaySum.GetSeconds() / stat.second.rxPackets) * 1000.0 : 0;
          double jitter = stat.second.rxPackets > 0 ? (stat.second.jitterSum.GetSeconds() / stat.second.rxPackets) * 1000.0 : 0;
          double packetLoss = stat.second.txPackets > 0 ? ((double)(stat.second.txPackets - stat.second.rxPackets) / stat.second.txPackets) * 100.0 : 0;
          double pdr = 100.0 - packetLoss;
          
          double downloadTime = throughput > 0 ? (1000.0 * 8.0) / throughput : 0; 
          double spectralEfficiency = throughput / 160.0;

          std::string jsonOutput = "{\n";
          jsonOutput += "  \"Technology\": \"Wi-Fi 6\",\n";
          jsonOutput += "  \"MIMO\": \"8x8\",\n";
          jsonOutput += "  \"MLO\": \"OFF\",\n";
          jsonOutput += "  \"QAM\": \"1024-QAM\",\n";
          jsonOutput += "  \"Channel Width\": \"160 MHz\",\n";
          jsonOutput += "  \"Throughput (Mbps)\": " + std::to_string(throughput) + ",\n";
          jsonOutput += "  \"Delay (ms)\": " + std::to_string(delay) + ",\n";
          jsonOutput += "  \"Jitter (ms)\": " + std::to_string(jitter) + ",\n";
          jsonOutput += "  \"Packet Loss (%)\": " + std::to_string(packetLoss) + ",\n";
          jsonOutput += "  \"Packet Delivery Ratio (%)\": " + std::to_string(pdr) + ",\n";
          jsonOutput += "  \"Download Time 1 GB (s)\": " + std::to_string(downloadTime) + ",\n";
          jsonOutput += "  \"Spectral Efficiency (bps/Hz)\": " + std::to_string(spectralEfficiency) + "\n";
          jsonOutput += "}\n";

          std::cout << jsonOutput;
          
          std::ofstream outFile("scratch/Pameran/ns3_hasil_wifi6.json");
          outFile << jsonOutput;
          outFile.close();
      }
  }

  Simulator::Destroy();
  return 0;
}
