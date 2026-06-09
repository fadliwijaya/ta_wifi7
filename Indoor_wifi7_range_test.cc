// KODE WIFI 7 MLO - Uji Jarak (Range / Pathloss Degradation)
#include "ns3/boolean.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-net-device.h"
#include "ns3/csma-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/hybrid-buildings-propagation-loss-model.h"
#include "ns3/buildings-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor.h"

#include <chrono>
#include <fstream>
#include <iostream>

using namespace ns3;

class TeeStream : public std::streambuf {
public:
  TeeStream(std::streambuf *sb1, std::streambuf *sb2) : sb1(sb1), sb2(sb2) {}

protected:
  virtual int overflow(int c) override {
    if (c == EOF) {
      return !EOF;
    } else {
      int const r1 = sb1 ? sb1->sputc(c) : EOF;
      if (sb2)
        sb2->sputc(c);
      return r1 == EOF ? EOF : c;
    }
  }
  virtual int sync() override {
    int const r1 = sb1 ? sb1->pubsync() : 0;
    if (sb2)
      sb2->pubsync();
    return r1 == 0 ? 0 : -1;
  }

private:
  std::streambuf *sb1;
  std::streambuf *sb2;
};

uint64_t g_rxBytes = 0;
uint32_t g_macDrops = 0;
std::ofstream g_csvFile;
double g_distanceLimit = 300.0;

Ptr<FlowMonitor> g_flowMonitor;
uint64_t g_prevRxPackets = 0;
uint64_t g_prevLostPackets = 0;
double g_prevDelaySum = 0.0;
double g_prevJitterSum = 0.0;
double g_currentRssi = -100.0;

void MonitorSniffRx(Ptr<const Packet> packet, uint16_t channelFreqMhz, WifiTxVector txVector, MpduInfo aMpdu, SignalNoiseDbm signalNoise, uint16_t staId) {
    g_currentRssi = signalNoise.signal;
}

void RxCallback(std::string path, Ptr<const Packet> packet, const Address& from) {
    g_rxBytes += packet->GetSize();
}

void MacDropCallback(std::string path, Ptr<const Packet> packet) {
    g_macDrops++;
}

void AdvancePosition(Ptr<Node> node, double stepSize, double stepTime) {
    Vector pos = node->GetObject<MobilityModel>()->GetPosition();
    
    double throughputMbps = (g_rxBytes * 8.0) / (stepTime * 1e6);
    g_rxBytes = 0; // reset

    double currentDelayMs = 0;
    double currentJitterMs = 0;
    uint64_t currentLost = 0;
    
    if (g_flowMonitor) {
        // Harus panggil CheckForLostPackets agar FlowMonitor mengupdate jumlah paket yang hilang/basi
        g_flowMonitor->CheckForLostPackets(); 
        std::map<FlowId, FlowMonitor::FlowStats> stats = g_flowMonitor->GetFlowStats();
        if (!stats.empty()) {
            auto stat = stats.begin()->second;
            uint64_t deltaRx = stat.rxPackets - g_prevRxPackets;
            currentLost = stat.lostPackets - g_prevLostPackets;
            
            if (deltaRx > 0) {
                currentDelayMs = ((stat.delaySum.GetSeconds() - g_prevDelaySum) / deltaRx) * 1000.0;
                currentJitterMs = ((stat.jitterSum.GetSeconds() - g_prevJitterSum) / deltaRx) * 1000.0;
            }
            g_prevRxPackets = stat.rxPackets;
            g_prevLostPackets = stat.lostPackets;
            g_prevDelaySum = stat.delaySum.GetSeconds();
            g_prevJitterSum = stat.jitterSum.GetSeconds();
        }
    }

    std::cout << "[INFO] Waktu: " << Simulator::Now().GetSeconds() 
              << "s | Jarak: " << pos.x << " m"
              << " | RSSI: " << g_currentRssi << " dBm"
              << " | Tput: " << throughputMbps << " Mbps"
              << " | Delay: " << currentDelayMs << " ms"
              << " | Jitter: " << currentJitterMs << " ms"
              << " | Drop: " << currentLost << std::endl;
              
    g_csvFile << Simulator::Now().GetSeconds() << "," 
              << pos.x << "," 
              << g_currentRssi << ","
              << throughputMbps << ","
              << currentDelayMs << ","
              << currentJitterMs << ","
              << currentLost << "\n";

    if (pos.x < g_distanceLimit) {
        pos.x += stepSize;
        node->GetObject<MobilityModel>()->SetPosition(pos);
        Simulator::Schedule(Seconds(stepTime), &AdvancePosition, node, stepSize, stepTime);
    }
}

int main(int argc, char *argv[]) {
  std::string outDir = "scratch/ta_wifi7/output_simulasi";
  if (std::system(("mkdir -p " + outDir).c_str()) != 0) { std::cerr << "Warning: Failed to create " << outDir << std::endl; }

  std::time_t t_now = std::time(nullptr);
  char time_str_now[100];
  std::strftime(time_str_now, sizeof(time_str_now), "%Y%m%d_%H%M%S", std::localtime(&t_now));
  std::string globalTimestamp(time_str_now);

  std::string logFilename = outDir + "/terminal_output_wifi7_range_" + globalTimestamp + ".log";
  std::ofstream logFile(logFilename);
  TeeStream tee(std::cout.rdbuf(), logFile.rdbuf());
  std::streambuf *oldCoutBuf = std::cout.rdbuf(&tee);

  std::string csvFilename = outDir + "/hasil_range_wifi7_" + globalTimestamp + ".csv";
  g_csvFile.open(csvFilename);
  g_csvFile << "Time_s,Distance_m,RSSI_dBm,Throughput_Mbps,Delay_ms,Jitter_ms,PacketDrop\n";

  char human_time_str[100];
  std::strftime(human_time_str, sizeof(human_time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&t_now));
  std::cout << "\n=======================================================\n";
  std::cout << "[INFO] TANGGAL & WAKTU EKSEKUSI SIMULASI : " << human_time_str << "\n";
  std::cout << "=======================================================\n";

  std::cout << "\n=======================================================\n";
  std::cout << "[INFO] Skenario Range / Pathloss Degradation (Wi-Fi 7)\n";
  std::cout << "=======================================================\n";
  std::cout << "[SPESIFIKASI SIMULASI]\n";
  std::cout << "- Standar Wi-Fi     : Wi-Fi 7 (802.11be)\n";
  std::cout << "- Konfigurasi MIMO  : 8x8 Spatial Streams\n";
  std::cout << "- Fitur Spesial     : Multi-Link Operation (MLO) Aktif\n";
  std::cout << "    * Link 0        : Frekuensi 5 GHz (Lebar Pita 160 MHz)\n";
  std::cout << "    * Link 1        : Frekuensi 6 GHz (Lebar Pita 320 MHz)\n";
  std::cout << "- Tx Power          : 20.0 dBm\n";
  std::cout << "- Rate Adaptation   : IdealWifiManager (Otomatis menyesuaikan hingga 4096-QAM)\n";
  std::cout << "- Propagation Model : HybridBuildingsPropagationLossModel (Redaman Tembok/Indoor)\n";
  std::cout << "- Jarak Uji         : 0 hingga " << g_distanceLimit << " meter\n";
  std::cout << "=======================================================\n\n";

  double simulationTimeSec = 32.0;
  
  Config::SetDefault("ns3::StaWifiMac::MaxMissedBeacons", UintegerValue(10)); // Hindari putus asosiasi prematur

  NodeContainer serverNode; serverNode.Create(1);
  NodeContainer wifiApNode; wifiApNode.Create(1);
  NodeContainer wifiStaNode; wifiStaNode.Create(1);

  MobilityHelper apMobility;
  Ptr<ListPositionAllocator> apPosAlloc = CreateObject<ListPositionAllocator>();
  apPosAlloc->Add(Vector(0.0, 0.0, 1.0));
  apMobility.SetPositionAllocator(apPosAlloc);
  apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  apMobility.Install(wifiApNode);

  MobilityHelper staMobility;
  Ptr<ListPositionAllocator> staPosAlloc = CreateObject<ListPositionAllocator>();
  staPosAlloc->Add(Vector(1.0, 0.0, 1.0));
  staMobility.SetPositionAllocator(staPosAlloc);
  staMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  staMobility.Install(wifiStaNode);

  BuildingsHelper::Install(wifiApNode);
  BuildingsHelper::Install(wifiStaNode);

  SpectrumWifiPhyHelper spectrumPhy(2); // MLO 2 Links
  spectrumPhy.SetErrorRateModel("ns3::NistErrorRateModel");
  spectrumPhy.Set("Antennas", UintegerValue(8));
  spectrumPhy.Set("TxPowerStart", DoubleValue(20.0));
  spectrumPhy.Set("TxPowerEnd", DoubleValue(20.0));
  spectrumPhy.Set("TxGain", DoubleValue(6.0));
  spectrumPhy.Set("RxGain", DoubleValue(6.0));

  Ptr<MultiModelSpectrumChannel> channel5Ghz = CreateObject<MultiModelSpectrumChannel>();
  Ptr<HybridBuildingsPropagationLossModel> loss5Ghz = CreateObject<HybridBuildingsPropagationLossModel>();
  loss5Ghz->SetAttribute("Frequency", DoubleValue(5500e6));
  channel5Ghz->AddPropagationLossModel(loss5Ghz);
  channel5Ghz->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
  spectrumPhy.Set(0, "ChannelSettings", StringValue("{0, 160, BAND_5GHZ, 0}"));
  spectrumPhy.AddChannel(channel5Ghz, WIFI_SPECTRUM_5_GHZ);

  Ptr<MultiModelSpectrumChannel> channel6Ghz = CreateObject<MultiModelSpectrumChannel>();
  Ptr<HybridBuildingsPropagationLossModel> loss6Ghz = CreateObject<HybridBuildingsPropagationLossModel>();
  loss6Ghz->SetAttribute("Frequency", DoubleValue(6025e6));
  channel6Ghz->AddPropagationLossModel(loss6Ghz);
  channel6Ghz->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
  spectrumPhy.Set(1, "ChannelSettings", StringValue("{0, 320, BAND_6GHZ, 0}"));
  spectrumPhy.AddChannel(channel6Ghz, WIFI_SPECTRUM_6_GHZ);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211be);
  wifi.SetRemoteStationManager("ns3::IdealWifiManager");

  WifiMacHelper macA;

  Ssid ssidA = Ssid("kampus-wifi");
  
  macA.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssidA));
  NetDeviceContainer staDeviceA = wifi.Install(spectrumPhy, macA, wifiStaNode);

  macA.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssidA));
  NetDeviceContainer apDeviceA = wifi.Install(spectrumPhy, macA, wifiApNode);

  for (uint32_t i = 0; i < apDeviceA.GetN(); ++i) {
      DynamicCast<WifiNetDevice>(apDeviceA.Get(i))->GetPhy()->SetMaxSupportedTxSpatialStreams(8);
      DynamicCast<WifiNetDevice>(apDeviceA.Get(i))->GetPhy()->SetMaxSupportedRxSpatialStreams(8);
  }
  for (uint32_t i = 0; i < staDeviceA.GetN(); ++i) {
      DynamicCast<WifiNetDevice>(staDeviceA.Get(i))->GetPhy()->SetMaxSupportedTxSpatialStreams(8);
      DynamicCast<WifiNetDevice>(staDeviceA.Get(i))->GetPhy()->SetMaxSupportedRxSpatialStreams(8);
  }

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("23Gbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));
  NodeContainer backboneNodes;
  backboneNodes.Add(serverNode);
  backboneNodes.Add(wifiApNode);
  NetDeviceContainer backboneDevices = csma.Install(backboneNodes);

  InternetStackHelper stack;
  stack.Install(serverNode);
  stack.Install(wifiApNode);
  stack.Install(wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer backboneInterfaces = address.Assign(backboneDevices);

  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ap0Interface = address.Assign(apDeviceA);
  Ipv4InterfaceContainer sta0Interface = address.Assign(staDeviceA);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> staticRoutingServer = ipv4RoutingHelper.GetStaticRouting(serverNode.Get(0)->GetObject<Ipv4>());
  staticRoutingServer->AddNetworkRouteTo(Ipv4Address("192.168.1.0"), Ipv4Mask("255.255.255.0"), backboneInterfaces.GetAddress(1), 1);

  Ptr<Ipv4StaticRouting> staticRoutingAp0 = ipv4RoutingHelper.GetStaticRouting(wifiApNode.Get(0)->GetObject<Ipv4>());
  staticRoutingAp0->SetDefaultRoute(backboneInterfaces.GetAddress(0), 1);

  Ptr<Ipv4StaticRouting> staticRoutingSta = ipv4RoutingHelper.GetStaticRouting(wifiStaNode.Get(0)->GetObject<Ipv4>());
  staticRoutingSta->SetDefaultRoute(ap0Interface.GetAddress(0), 1);

  // UDP Traffic
  uint16_t port = 9000;
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(wifiStaNode.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(simulationTimeSec));

  // Injeksi masif 5Gbps untuk menguji puncak kapasitas
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(sta0Interface.GetAddress(0), port));
  onoff.SetAttribute("DataRate", StringValue("5Gbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(1472));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  
  ApplicationContainer clientApps = onoff.Install(serverNode.Get(0));
  clientApps.Start(Seconds(0.5));
  clientApps.Stop(Seconds(simulationTimeSec));

  // Connect PacketSink RX to counter
  Config::Connect("/NodeList/2/ApplicationList/0/$ns3::PacketSink/Rx", MakeCallback(&RxCallback));
  // Connect MonitorSnifferRx for RSSI (Node 2 is STA)
  Config::ConnectWithoutContext("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/MonitorSnifferRx", MakeCallback(&MonitorSniffRx));

  FlowMonitorHelper flowmon;
  g_flowMonitor = flowmon.InstallAll();

  Simulator::Schedule(Seconds(0.2), &AdvancePosition, wifiStaNode.Get(0), 2.0, 0.2);

  auto startRealTime = std::chrono::high_resolution_clock::now();

  Simulator::Stop(Seconds(simulationTimeSec));
  Simulator::Run();

  auto endRealTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> diffRealTime = endRealTime - startRealTime;
  double totalSeconds = diffRealTime.count();
  double totalMinutes = totalSeconds / 60.0;
  std::cout << "[INFO] Waktu Eksekusi Nyata (Real Runtime) Simulasi: "
            << totalMinutes << " menit (" << totalSeconds << " detik)\n" << std::endl;

  g_csvFile.close();
  Simulator::Destroy();
  
  std::cout.rdbuf(oldCoutBuf);
  logFile.close();

  return 0;
}
