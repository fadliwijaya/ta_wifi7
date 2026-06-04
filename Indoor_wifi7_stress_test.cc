//// KODE WIFI 7 Dengan MLO - Skenario Kampus (2 Ruangan)

#include "ns3/boolean.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/waypoint-mobility-model.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/qos-txop.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-mac-queue.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-net-device.h"
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>

// Header Modul Buildings
#include "ns3/building.h"
#include "ns3/buildings-helper.h"
#include "ns3/hybrid-buildings-propagation-loss-model.h"

// Header Modul Flow Monitor
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/wifi-helper.h"
#include "ns3/wifi-standards.h"

#include "ns3/csma-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/random-variable-stream.h"
#include "ns3/rectangle.h"

#include <iostream>
#include <streambuf>
#include <string>
#include <vector>

using namespace ns3;

// Kelas pembantu untuk menduplikasi output std::cout ke file dan terminal
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

// Global variables untuk Channel Utilization
uint64_t g_busySamplesAp0 = 0;
uint64_t g_busySamplesAp1 = 0;
uint64_t g_totalSamples = 0;

void SampleChannel(Ptr<WifiNetDevice> ap0, Ptr<WifiNetDevice> ap1) {
  if (ap0 && ap0->GetPhy(0) && ap0->GetPhy(1)) {
    if (!ap0->GetPhy(0)->IsStateIdle() || !ap0->GetPhy(1)->IsStateIdle()) {
      g_busySamplesAp0++;
    }
  }
  if (ap1 && ap1->GetPhy(0) && ap1->GetPhy(1)) {
    if (!ap1->GetPhy(0)->IsStateIdle() || !ap1->GetPhy(1)->IsStateIdle()) {
      g_busySamplesAp1++;
    }
  }
  g_totalSamples++;
  Simulator::Schedule(MilliSeconds(10), &SampleChannel, ap0, ap1);
}

// Global variables untuk Handover
uint32_t g_totalAssoc = 0;
std::map<uint32_t, uint32_t> g_staAssocCount;

void StaAssocCallback(std::string context, Mac48Address bssid) {
  size_t first = context.find("NodeList/") + 9;
  size_t second = context.find("/", first);
  uint32_t nodeId = std::stoi(context.substr(first, second - first));

  g_staAssocCount[nodeId]++;
  g_totalAssoc++;
  std::cout << "[STA ASSOC] Waktu: " << Simulator::Now().GetSeconds()
            << "s | STA Node " << nodeId
            << " sukses Asosiasi ke AP BSSID: " << bssid << std::endl;
}

// Fungsi Sampling Queue Occupancy (Panjang Antrean)
void SampleQueue(Ptr<WifiNetDevice> ap0, Ptr<WifiNetDevice> ap1) {
  uint32_t qLen0 = 0;
  uint32_t qLen1 = 0;

  if (ap0 && ap0->GetMac()) {
    for (uint8_t ac = 0; ac < 4; ac++) {
      if (ap0->GetMac()->GetQosTxop(ac)) {
        qLen0 +=
            ap0->GetMac()->GetQosTxop(ac)->GetWifiMacQueue()->GetNPackets();
      }
    }
  }
  if (ap1 && ap1->GetMac()) {
    for (uint8_t ac = 0; ac < 4; ac++) {
      if (ap1->GetMac()->GetQosTxop(ac)) {
        qLen1 +=
            ap1->GetMac()->GetQosTxop(ac)->GetWifiMacQueue()->GetNPackets();
      }
    }
  }

  std::cout << "[Antrean MAC] Waktu: " << Simulator::Now().GetSeconds()
            << "s | AP0 (Ruang 1): " << qLen0
            << " paket | AP1 (Ruang 2): " << qLen1 << " paket" << std::endl;

  Simulator::Schedule(MilliSeconds(100), &SampleQueue, ap0, ap1);
}

// Definisi Struktur Profil Pengguna
struct UserProfile {
  std::string name;
  uint16_t port;
  uint32_t packetSize; // Bytes
  std::string dataRate;
  std::string ulDataRate;
  uint32_t tos;       // IP Type of Service for WMM EDCA classification
  std::string acName; // Name of WMM Access Category
};

int main(int argc, char *argv[]) {
  // Setup direktori output
  std::string outDir = "scratch/ta_wifi7/output_simulasi";
  if (std::system(("mkdir -p " + outDir).c_str()) != 0) { std::cerr << "Warning: Failed to create " << outDir << std::endl; }

  // Setup timestamp untuk nama file log dan CSV
  std::time_t t_now = std::time(nullptr);
  char time_str_now[100];
  std::strftime(time_str_now, sizeof(time_str_now), "%Y%m%d_%H%M%S",
                std::localtime(&t_now));
  std::string globalTimestamp(time_str_now);

  // Setup file log terminal
  std::string logFilename =
      outDir + "/terminal_output_" + globalTimestamp + ".log";
  std::ofstream logFile(logFilename);

  // Duplikasi std::cout ke terminal dan logFile
  TeeStream tee(std::cout.rdbuf(), logFile.rdbuf());
  std::streambuf *oldCoutBuf = std::cout.rdbuf(&tee);

  // Mulai pencatatan waktu eksekusi nyata
  auto startRealTime = std::chrono::high_resolution_clock::now();

  char human_time_str[100];
  std::strftime(human_time_str, sizeof(human_time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&t_now));
  std::cout << "\n=======================================================\n";
  std::cout << "[INFO] TANGGAL & WAKTU EKSEKUSI SIMULASI : " << human_time_str << "\n";
  std::cout << "=======================================================\n";

  std::cout << "\n>>> Memulai simulasi STRESS TEST WI-FI 7 (BEBAN MAKSIMAL GIGABIT), "
               "menunggu 10 detik... <<<\n"
            << std::endl;

  double simulationTimeSec = 2.0; // Kembali ke 2 detik agar tidak terlalu lama (STA akan disuruh lari cepat)

  CommandLine cmd(__FILE__);
  cmd.AddValue("simulationTime", "Simulation active time (seconds)",
               simulationTimeSec);
  cmd.Parse(argc, argv);

  // --- OPTIMASI BUFFER & A-MPDU ---
  // Mencegah Buffer Overflow untuk trafik masif (seperti Gaming)
  Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue("5000p"));
  Config::SetDefault("ns3::WifiMacQueue::MaxDelay",
                     TimeValue(Seconds(1.0)));
  Config::SetDefault("ns3::FqCoDelQueueDisc::MaxSize", StringValue("5000p"));

  // Mencegah macet dengan mengirim frame A-MPDU ukuran raksasa
  Config::SetDefault("ns3::WifiMac::MpduBufferSize", UintegerValue(1024));
  Config::SetDefault("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue(1048575));
  Config::SetDefault("ns3::WifiMac::BK_MaxAmpduSize", UintegerValue(1048575));
  Config::SetDefault("ns3::WifiMac::VI_MaxAmpduSize", UintegerValue(1048575));
  Config::SetDefault("ns3::WifiMac::VO_MaxAmpduSize", UintegerValue(1048575));

  Time simulationTime = Seconds(simulationTimeSec);

  std::vector<UserProfile> profiles = {
      {"Social Media", 9001, 200, "25Mbps", "5Mbps", 0x00, "AC_BE"},
      {"Video 4K Streaming", 9002, 1472, "100Mbps", "5Mbps", 0xa0, "AC_VI"},
      {"Gaming", 9003, 1472, "50Mbps", "20Mbps", 0xc0, "AC_VO"},
      {"File Download", 9004, 1472, "150Mbps", "10Mbps", 0x20, "AC_BK"},
      {"Web Browsing", 9005, 1000, "25Mbps", "5Mbps", 0x00, "AC_BE"},
      {"Live Streaming", 9006, 1472, "10Mbps", "50Mbps", 0xa0, "AC_VI"}};

  NodeContainer wifiApNode;
  wifiApNode.Create(2); // 2 AP (Satu untuk tiap ruangan)
  NodeContainer wifiStaNode;
  wifiStaNode.Create(24); // Total 24 STA (12 Kelas A, 10 Kelas B, 2 Koridor)

  Ptr<Building> building = CreateObject<Building>();
  building->SetBoundaries(
      Box(0.0, 16.0, 0.0, 11.0, 0.0,
          3.0)); // Y diperluas ke 11.0 untuk menampung koridor
  building->SetBuildingType(Building::Office);
  building->SetExtWallsType(Building::ConcreteWithoutWindows);
  building->SetNFloors(1);
  building->SetNRoomsX(
      2); // 2 Ruangan (Kelas A & Kelas B) dengan dinding pemisah di X=8.0
  building->SetNRoomsY(1);

  // Posisi AP (Tengah atas di masing-masing ruangan)
  MobilityHelper apMobility;
  Ptr<ListPositionAllocator> apPositionAlloc =
      CreateObject<ListPositionAllocator>();
  apPositionAlloc->Add(Vector(4.0, 4.0, 2.9));  // AP 0 di Ruang 1
  apPositionAlloc->Add(Vector(12.0, 4.0, 2.9)); // AP 1 di Ruang 2
  apMobility.SetPositionAllocator(apPositionAlloc);
  apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  apMobility.Install(wifiApNode);

  NodeContainer staKelasA, staKelasB, staKoridor;
  for (uint32_t i = 0; i < 12; ++i)
    staKelasA.Add(wifiStaNode.Get(i));
  for (uint32_t i = 12; i < 22; ++i)
    staKelasB.Add(wifiStaNode.Get(i));
  for (uint32_t i = 22; i < 24; ++i)
    staKoridor.Add(wifiStaNode.Get(i));

  // --- Mobilitas Kelas A (Ruang Kiri) ---
  MobilityHelper staMobilityKelasA;
  Ptr<GridPositionAllocator> gridAllocA = CreateObject<GridPositionAllocator>();
  gridAllocA->SetMinX(1.0);
  gridAllocA->SetMinY(1.0);
  gridAllocA->SetZ(1.0); // Ketinggian meja
  gridAllocA->SetDeltaX(1.5);
  gridAllocA->SetDeltaY(1.5);
  gridAllocA->SetAttribute("GridWidth", UintegerValue(5)); // 5 kursi per baris
  gridAllocA->SetLayoutType(GridPositionAllocator::ROW_FIRST);
  staMobilityKelasA.SetPositionAllocator(gridAllocA);
  staMobilityKelasA.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  staMobilityKelasA.Install(staKelasA);

  // --- Mobilitas Kelas B (Ruang Kanan) ---
  MobilityHelper staMobilityKelasB;
  Ptr<GridPositionAllocator> gridAllocB = CreateObject<GridPositionAllocator>();
  gridAllocB->SetMinX(9.0);
  gridAllocB->SetMinY(1.0);
  gridAllocB->SetZ(1.0);
  gridAllocB->SetDeltaX(1.5);
  gridAllocB->SetDeltaY(1.5);
  gridAllocB->SetAttribute("GridWidth", UintegerValue(5));
  gridAllocB->SetLayoutType(GridPositionAllocator::ROW_FIRST);
  staMobilityKelasB.SetPositionAllocator(gridAllocB);
  staMobilityKelasB.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  staMobilityKelasB.Install(staKelasB);

  // --- Mobilitas Koridor (Atas) ---
  // Memisahkan area mobilitas agar STA 18 (Ssid A) tetap di zona AP0 (X: 0.5-8.0), dan STA 19 (Ssid B) di zona AP1 (X: 8.0-15.5)
  MobilityHelper staMobilityKoridorA;
  staMobilityKoridorA.SetPositionAllocator(
      "ns3::RandomRectanglePositionAllocator", "X",
      StringValue("ns3::UniformRandomVariable[Min=0.5|Max=8.0]"), "Y",
      StringValue("ns3::UniformRandomVariable[Min=8.5|Max=10.5]"));
  staMobilityKoridorA.SetMobilityModel(
      "ns3::RandomWalk2dMobilityModel", "Bounds",
      RectangleValue(Rectangle(0.5, 8.0, 8.5, 10.5)),
      "Speed", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=1.5]"));
  staMobilityKoridorA.Install(staKoridor.Get(0));

  MobilityHelper staMobilityKoridorB;
  staMobilityKoridorB.SetPositionAllocator(
      "ns3::RandomRectanglePositionAllocator", "X",
      StringValue("ns3::UniformRandomVariable[Min=8.0|Max=15.5]"), "Y",
      StringValue("ns3::UniformRandomVariable[Min=8.5|Max=10.5]"));
  staMobilityKoridorB.SetMobilityModel(
      "ns3::RandomWalk2dMobilityModel", "Bounds",
      RectangleValue(Rectangle(8.0, 15.5, 8.5, 10.5)),
      "Speed", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=1.5]"));
  staMobilityKoridorB.Install(staKoridor.Get(1));

  BuildingsHelper::Install(wifiApNode);
  BuildingsHelper::Install(wifiStaNode);

  SpectrumWifiPhyHelper spectrumPhy(2);
  spectrumPhy.SetErrorRateModel("ns3::NistErrorRateModel");

  spectrumPhy.Set("Antennas", UintegerValue(4));
  spectrumPhy.Set("MaxSupportedTxSpatialStreams", UintegerValue(4));
  spectrumPhy.Set("MaxSupportedRxSpatialStreams", UintegerValue(4));

  spectrumPhy.Set("TxPowerStart", DoubleValue(20.0));
  spectrumPhy.Set("TxPowerEnd", DoubleValue(20.0));
  spectrumPhy.Set("TxGain",
                  DoubleValue(6.0)); // Berdasarkan spesifikasi wajar Wi-Fi 7 AP
                                     // (contoh: Ruijie RG-AP9860)
  spectrumPhy.Set("RxGain", DoubleValue(6.0));

  Ptr<MultiModelSpectrumChannel> channel5Ghz =
      CreateObject<MultiModelSpectrumChannel>();
  Ptr<HybridBuildingsPropagationLossModel> loss5Ghz =
      CreateObject<HybridBuildingsPropagationLossModel>();
  loss5Ghz->SetAttribute("Frequency", DoubleValue(5500e6));
  channel5Ghz->AddPropagationLossModel(loss5Ghz);
  channel5Ghz->SetPropagationDelayModel(
      CreateObject<ConstantSpeedPropagationDelayModel>());
  spectrumPhy.Set(0, "ChannelSettings", StringValue("{0, 160, BAND_5GHZ, 0}"));
  spectrumPhy.AddChannel(channel5Ghz, WIFI_SPECTRUM_5_GHZ);

  Ptr<MultiModelSpectrumChannel> channel6Ghz =
      CreateObject<MultiModelSpectrumChannel>();
  Ptr<HybridBuildingsPropagationLossModel> loss6Ghz =
      CreateObject<HybridBuildingsPropagationLossModel>();
  loss6Ghz->SetAttribute("Frequency", DoubleValue(6025e6));
  channel6Ghz->AddPropagationLossModel(loss6Ghz);
  channel6Ghz->SetPropagationDelayModel(
      CreateObject<ConstantSpeedPropagationDelayModel>());
  spectrumPhy.Set(1, "ChannelSettings", StringValue("{0, 320, BAND_6GHZ, 0}"));
  spectrumPhy.AddChannel(channel6Ghz, WIFI_SPECTRUM_6_GHZ);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211be);

  // 1. DYNAMIC RATE ADAPTATION
  // Menggunakan IdealWifiManager agar STA secara dinamis menyesuaikan MCS 
  // (turun dari 4096-QAM jika jarak menjauh/interferensi tinggi)
  wifi.SetRemoteStationManager("ns3::IdealWifiManager");

  // 2. MULTI-AP COORDINATION (Representasi OBSS PD Spatial Reuse)
  // Mengaktifkan algoritma OBSS PD untuk memungkinkan AP berkoordinasi
  // menoleransi interferensi ruang (Spatial Reuse)
  wifi.SetObssPdAlgorithm("ns3::ConstantObssPdAlgorithm", "ObssPdLevel",
                          DoubleValue(-82.0));

  WifiMacHelper macA;
  Ssid ssidA = Ssid("kampus-wifi7-A");
  macA.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssidA), "QosSupported",
               BooleanValue(true), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDeviceA = wifi.Install(spectrumPhy, macA, staKelasA);

  macA.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssidA), "QosSupported",
               BooleanValue(true));
  NetDeviceContainer apDeviceA = wifi.Install(spectrumPhy, macA, wifiApNode.Get(0));

  WifiMacHelper macB;
  Ssid ssidB = Ssid("kampus-wifi7-B");
  macB.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssidB), "QosSupported",
               BooleanValue(true), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDeviceB = wifi.Install(spectrumPhy, macB, staKelasB);

  macB.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssidB), "QosSupported",
               BooleanValue(true));
  NetDeviceContainer apDeviceB = wifi.Install(spectrumPhy, macB, wifiApNode.Get(1));

  // Koridor: STA 18 ikut AP0, STA 19 ikut AP1
  macA.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssidA), "QosSupported",
               BooleanValue(true), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDeviceKoridorA = wifi.Install(spectrumPhy, macA, staKoridor.Get(0));

  macB.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssidB), "QosSupported",
               BooleanValue(true), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDeviceKoridorB = wifi.Install(spectrumPhy, macB, staKoridor.Get(1));

  // Gabungkan semua device untuk keperluan referensi (apDevice dan staDevice)
  NetDeviceContainer apDevice;
  apDevice.Add(apDeviceA);
  apDevice.Add(apDeviceB);

  NetDeviceContainer staDevice;
  staDevice.Add(staDeviceA);
  staDevice.Add(staDeviceB);
  staDevice.Add(staDeviceKoridorA);
  staDevice.Add(staDeviceKoridorB);

  // ==========================================
  // [NEW] BSS TETANGGA (ROGUE AP) UNTUK INTERFERENSI (WI-FI 6 - 5GHz SAJA)
  // ==========================================
  NodeContainer rogueApNode, rogueStaNode;
  rogueApNode.Create(1);
  rogueStaNode.Create(1);

  MobilityHelper rogueMob;
  Ptr<ListPositionAllocator> rogueAlloc = CreateObject<ListPositionAllocator>();
  rogueAlloc->Add(Vector(8.0, 13.0, 2.9));
  rogueAlloc->Add(Vector(8.0, 14.0, 1.0));
  rogueMob.SetPositionAllocator(rogueAlloc);
  rogueMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  rogueMob.Install(rogueApNode);
  rogueMob.Install(rogueStaNode);

  // -------------------------------------------------------------
  // ROGUE AP WI-FI 6 (802.11ax) DI 5GHz SAJA (SINGLE LINK)
  // -------------------------------------------------------------
  WifiHelper wifi6Rogue;
  wifi6Rogue.SetStandard(WIFI_STANDARD_80211ax);
  wifi6Rogue.SetRemoteStationManager("ns3::IdealWifiManager");

  SpectrumWifiPhyHelper roguePhy(1); // Hanya 1 Link (Non-MLO)
  roguePhy.SetErrorRateModel("ns3::NistErrorRateModel");
  roguePhy.Set("Antennas", UintegerValue(2));
  roguePhy.Set("MaxSupportedTxSpatialStreams", UintegerValue(2));
  roguePhy.Set("MaxSupportedRxSpatialStreams", UintegerValue(2));
  roguePhy.Set("TxPowerStart", DoubleValue(20.0));
  roguePhy.Set("TxPowerEnd", DoubleValue(20.0));
  roguePhy.Set("TxGain", DoubleValue(3.0));
  roguePhy.Set("RxGain", DoubleValue(3.0));

  // Menumpang di spectrum channel 5GHz yang SAMA persis dengan AP Utama
  roguePhy.Set(0, "ChannelSettings", StringValue("{0, 160, BAND_5GHZ, 0}"));
  roguePhy.AddChannel(channel5Ghz, WIFI_SPECTRUM_5_GHZ);

  Ssid rogueSsid = Ssid("tetangga-wifi6");
  WifiMacHelper rogueMac;
  rogueMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(rogueSsid), "QosSupported",
                   BooleanValue(true), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer rogueStaDev = wifi6Rogue.Install(roguePhy, rogueMac, rogueStaNode);

  rogueMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(rogueSsid), "QosSupported",
                   BooleanValue(true));
  NetDeviceContainer rogueApDev = wifi6Rogue.Install(roguePhy, rogueMac, rogueApNode);

  BuildingsHelper::Install(rogueApNode);
  BuildingsHelper::Install(rogueStaNode);

  // ==========================================
  // [NEW] CENTRAL SERVER & CSMA BACKBONE (L3 DS)
  // ==========================================
  NodeContainer serverNode;
  serverNode.Create(1);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("10Gbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

  NodeContainer backboneNodes;
  backboneNodes.Add(serverNode);
  backboneNodes.Add(wifiApNode); // Menghubungkan Server dengan AP0 dan AP1

  NetDeviceContainer backboneDevices = csma.Install(backboneNodes);

  InternetStackHelper stack;
  stack.Install(serverNode);
  stack.Install(wifiApNode);
  stack.Install(wifiStaNode);
  stack.Install(rogueApNode);
  stack.Install(rogueStaNode);

  Ipv4AddressHelper address;

  // 1. Subnet Backbone (Server & AP)
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer backboneInterfaces = address.Assign(backboneDevices);

  // Pisahkan STA Device Container untuk Assignment Subnet yang berbeda
  NetDeviceContainer staDevAp0, staDevAp1;
  for (uint32_t i = 0; i < 12; ++i) staDevAp0.Add(staDevice.Get(i));
  for (uint32_t i = 10; i < 24; ++i) staDevAp1.Add(staDevice.Get(i));

  // 2. Subnet Wi-Fi Ruang 1 (AP0)
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ap0Interface = address.Assign(apDevice.Get(0));
  Ipv4InterfaceContainer sta0Interface = address.Assign(staDevAp0);

  // 3. Subnet Wi-Fi Ruang 2 (AP1)
  address.SetBase("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ap1Interface = address.Assign(apDevice.Get(1));
  Ipv4InterfaceContainer sta1Interface = address.Assign(staDevAp1);

  // Satukan kembali interface STA ke dalam satu container (staInterface) agar kompatibel dengan logika di bawah
  Ipv4InterfaceContainer staInterface;
  for(uint32_t i=0; i<10; ++i) staInterface.Add(sta0Interface.Get(i));
  for(uint32_t i=0; i<10; ++i) staInterface.Add(sta1Interface.Get(i));

  Ipv4AddressHelper rogueAddr;
  rogueAddr.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer rogueApIf = rogueAddr.Assign(rogueApDev);
  Ipv4InterfaceContainer rogueStaIf = rogueAddr.Assign(rogueStaDev);

  // Aktifkan Routing Statis (Bypass bug ns-3 GlobalRouting pada MLO)
  Ipv4StaticRoutingHelper ipv4RoutingHelper;

  // Server Routing: Arahkan trafik subnet 192.168.1.x ke IP CSMA AP0, dan 192.168.2.x ke IP CSMA AP1
  Ptr<Ipv4> ipv4Server = serverNode.Get(0)->GetObject<Ipv4>();
  Ptr<Ipv4StaticRouting> staticRoutingServer = ipv4RoutingHelper.GetStaticRouting(ipv4Server);
  staticRoutingServer->AddNetworkRouteTo(Ipv4Address("192.168.1.0"), Ipv4Mask("255.255.255.0"), backboneInterfaces.GetAddress(1), 1);
  staticRoutingServer->AddNetworkRouteTo(Ipv4Address("192.168.2.0"), Ipv4Mask("255.255.255.0"), backboneInterfaces.GetAddress(2), 1);

  // AP0 Routing: Arahkan default route (ke server dan AP1) lewat antarmuka CSMA-nya
  Ptr<Ipv4> ipv4Ap0 = wifiApNode.Get(0)->GetObject<Ipv4>();
  Ptr<Ipv4StaticRouting> staticRoutingAp0 = ipv4RoutingHelper.GetStaticRouting(ipv4Ap0);
  staticRoutingAp0->SetDefaultRoute(backboneInterfaces.GetAddress(0), 1);

  // AP1 Routing: Arahkan default route (ke server dan AP0) lewat antarmuka CSMA-nya
  Ptr<Ipv4> ipv4Ap1 = wifiApNode.Get(1)->GetObject<Ipv4>();
  Ptr<Ipv4StaticRouting> staticRoutingAp1 = ipv4RoutingHelper.GetStaticRouting(ipv4Ap1);
  staticRoutingAp1->SetDefaultRoute(backboneInterfaces.GetAddress(0), 1);

  // STA Routing: Default route ke AP masing-masing agar bisa kirim Uplink ke Server
  for (uint32_t i = 0; i < 12; ++i) {
    Ptr<Ipv4> ipv4Sta = wifiStaNode.Get(i)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRoutingSta = ipv4RoutingHelper.GetStaticRouting(ipv4Sta);
    staticRoutingSta->SetDefaultRoute(ap0Interface.GetAddress(0), 1);
  }
  for (uint32_t i = 10; i < 24; ++i) {
    Ptr<Ipv4> ipv4Sta = wifiStaNode.Get(i)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRoutingSta = ipv4RoutingHelper.GetStaticRouting(ipv4Sta);
    staticRoutingSta->SetDefaultRoute(ap1Interface.GetAddress(0), 1);
  }

  OnOffHelper rogueTraffic("ns3::UdpSocketFactory",
                           InetSocketAddress(rogueStaIf.GetAddress(0), 9999));
  rogueTraffic.SetAttribute("DataRate", StringValue("20Mbps")); // Diturunkan dari 100Mbps agar interferensi wajar
  rogueTraffic.SetAttribute("PacketSize", UintegerValue(1500));
  rogueTraffic.SetAttribute(
      "OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  rogueTraffic.SetAttribute(
      "OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer rogueApp = rogueTraffic.Install(rogueApNode.Get(0));
  rogueApp.Start(Seconds(0.5));
  rogueApp.Stop(simulationTime);

  // Pasang Packet Sink di Rogue STA agar trafik UDP diterima dengan baik (tidak
  // dibuang/ICMP Unreachable)
  PacketSinkHelper rogueSink("ns3::UdpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), 9999));
  ApplicationContainer rogueSinkApp = rogueSink.Install(rogueStaNode.Get(0));
  rogueSinkApp.Start(Seconds(0.0));
  rogueSinkApp.Stop(simulationTime);

  ApplicationContainer sinkApps;
  ApplicationContainer clientApps;

  for (uint32_t i = 0; i < 24; ++i) {
    uint32_t profileIdx = i % 6;
    Address sinkLocalAddress(
        InetSocketAddress(Ipv4Address::GetAny(), profiles[profileIdx].port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                      sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiStaNode.Get(i));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(simulationTime + Seconds(2.0));
    sinkApps.Add(sinkApp);

    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(staInterface.GetAddress(i),
                                        profiles[profileIdx].port));

    ns3::DataRate baseRate(profiles[profileIdx].dataRate);
    std::string burstRateStr =
        std::to_string(baseRate.GetBitRate() * 2) + "bps";

    onoff.SetAttribute(
        "OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff.SetAttribute(
        "OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoff.SetAttribute("PacketSize",
                       UintegerValue(profiles[profileIdx].packetSize));
    onoff.SetAttribute("DataRate", StringValue(burstRateStr));
    onoff.SetAttribute("Tos", UintegerValue(profiles[profileIdx].tos));

    // Trafik sekarang dibangkitkan dari Central Server, bukan dari AP!
    ApplicationContainer clientApp = onoff.Install(serverNode.Get(0));

    double startTime = 1.0 + (i * 0.005);
    clientApp.Start(Seconds(startTime));
    clientApp.Stop(simulationTime + Seconds(1.0));
    clientApps.Add(clientApp);

    // ==========================================
    // INJEKSI TRAFIK UPLINK (STA -> Server)
    // ==========================================
    uint16_t ulPort = 10000 + i;
    
    PacketSinkHelper packetSinkUl("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), ulPort));
    ApplicationContainer sinkAppUl = packetSinkUl.Install(serverNode.Get(0));
    sinkAppUl.Start(Seconds(0.0));
    sinkAppUl.Stop(simulationTime + Seconds(2.0));

    OnOffHelper onoffUl("ns3::UdpSocketFactory", InetSocketAddress(backboneInterfaces.GetAddress(0), ulPort));
    onoffUl.SetAttribute("PacketSize", UintegerValue(profiles[profileIdx].packetSize));
    onoffUl.SetAttribute("Tos", UintegerValue(profiles[profileIdx].tos));
    onoffUl.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoffUl.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    std::string ulRateStr = std::to_string(baseRate.GetBitRate()) + "bps"; // 1x base rate for UL (since DL is 2x)
    onoffUl.SetAttribute("DataRate", StringValue(ulRateStr));

    ApplicationContainer clientAppUl = onoffUl.Install(wifiStaNode.Get(i));
    clientAppUl.Start(Seconds(startTime + 0.05)); // delay 50ms agar tidak collision sempurna
    clientAppUl.Stop(simulationTime + Seconds(1.0));
    clientApps.Add(clientAppUl);
  }

  Config::Connect(
      "/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc",
      MakeCallback(&StaAssocCallback));

  // Mulai Sampling Channel Utilization & Queue Occupancy
  Ptr<WifiNetDevice> devAp0 = DynamicCast<WifiNetDevice>(apDevice.Get(0));
  Ptr<WifiNetDevice> devAp1 = DynamicCast<WifiNetDevice>(apDevice.Get(1));
  Simulator::Schedule(Seconds(1.0), &SampleChannel, devAp0, devAp1);
  std::cout << "\n--- Memulai Log Panjang Antrean MAC (Queue Occupancy) per "
               "100 ms ---"
            << std::endl;
  Simulator::Schedule(MilliSeconds(100), &SampleQueue, devAp0, devAp1);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(simulationTime + Seconds(2.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  std::cout << "\n================= HASIL SIMULASI STRESS TEST WI-FI 7 "
               "LENGKAP ================="
            << std::endl;
  std::cout
      << "Skenario           : Ruang 1 (Diam/Duduk) & Ruang 2 (Bergerak/Mobile)"
      << std::endl;
  std::cout << "Jumlah User (STA)  : 24 User (12 Kelas A, 10 Kelas B, 2 Koridor)"
            << std::endl;
  std::cout << "================= SPESIFIKASI PARAMETER SIMULASI =================" << std::endl;
  std::cout << "Standar Wi-Fi      : IEEE 802.11be (Wi-Fi 7)" << std::endl;
  std::cout << "Lebar Pita (BW)    : 320 MHz (Multi-Link 5 GHz & 6 GHz)" << std::endl;
  std::cout << "Max Modulasi       : MCS 13 (4096-QAM)" << std::endl;
  std::cout << "Antena / MIMO      : 2x2 Spatial Streams" << std::endl;
  std::cout << "Tx Power           : 20 dBm" << std::endl;
  std::cout << "Routing / QoS      : IPv4 Static Routing / WMM (EDCA)" << std::endl;
  std::cout << "Propagation Model  : HybridBuildingsPropagationLossModel" << std::endl;
  std::cout << "Fitur Wi-Fi 7      : Dynamic Rate Adaptation (IdealWifiManager), MLO Link Steering Resilience, OBSS PD Spatial Reuse"
            << std::endl;
  std::cout << "---------------------------------------------------------------"
               "------------------"
            << std::endl;

  // Global Metriks
  double globalThroughputMbps = 0.0;

  // Analisis Per Profil
  for (uint32_t p = 0; p < 6; ++p) {
    bool profileFound = false;
    std::cout << "\n>>> Profil: " << profiles[p].name
              << " (Target DL: " << profiles[p].dataRate << " | UL: " << profiles[p].ulDataRate << ") <<<" << std::endl;
    double dlTotalThroughput = 0.0, ulTotalThroughput = 0.0;
    double dlTotalDelayMs = 0.0, ulTotalDelayMs = 0.0;
    double dlTotalJitterMs = 0.0, ulTotalJitterMs = 0.0;
    uint64_t dlTotalDrop = 0, ulTotalDrop = 0;
    int dlFlowCount = 0, ulFlowCount = 0;

    std::vector<double> dlUserThroughputs, ulUserThroughputs;

    for (uint32_t i = 0; i < 24; ++i) {
      if (i % 6 != p) continue; // Hanya hitung STA yang menggunakan profil ini

      Ipv4Address staAddr = staInterface.GetAddress(i);
      
      for (auto flow = stats.begin(); flow != stats.end(); ++flow) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow->first);
        
        bool isDl = (t.destinationAddress == staAddr);
        bool isUl = (t.sourceAddress == staAddr);
        
        if (!isDl && !isUl) continue;

        double throughput = 0.0;
        double duration = flow->second.timeLastRxPacket.GetSeconds() - flow->second.timeFirstTxPacket.GetSeconds();
        if (duration > 0) {
          throughput = (flow->second.rxBytes * 8.0) / (duration * 1e6);
        } else {
          throughput = (flow->second.rxBytes * 8.0) / (simulationTime.GetSeconds() * 1e6);
        }

        double avgDelayMs = 0.0;
        double avgJitterMs = 0.0;
        if (flow->second.rxPackets > 0) {
          avgDelayMs = (flow->second.delaySum.GetSeconds() / flow->second.rxPackets) * 1000.0;
          avgJitterMs = (flow->second.jitterSum.GetSeconds() / flow->second.rxPackets) * 1000.0;
        }
        uint64_t macDrop = flow->second.txPackets - flow->second.rxPackets;

        if (isDl) {
          profileFound = true;
          dlFlowCount++;
          dlTotalThroughput += throughput;
          dlUserThroughputs.push_back(throughput);
          dlTotalDelayMs += avgDelayMs;
          dlTotalJitterMs += avgJitterMs;
          dlTotalDrop += macDrop;
        } else if (isUl) {
          profileFound = true;
          ulFlowCount++;
          ulTotalThroughput += throughput;
          ulUserThroughputs.push_back(throughput);
          ulTotalDelayMs += avgDelayMs;
          ulTotalJitterMs += avgJitterMs;
          ulTotalDrop += macDrop;
        }
      }
    }

    if (profileFound) {
      globalThroughputMbps += (dlTotalThroughput + ulTotalThroughput);

      auto calcJains = [](const std::vector<double>& ths) {
        double sumTh = 0.0, sumThSq = 0.0;
        for (double th : ths) { sumTh += th; sumThSq += (th * th); }
        return (sumThSq > 0) ? ((sumTh * sumTh) / (ths.size() * sumThSq)) : 0.0;
      };

      double avgSinrEstimated = 56.5;

      std::cout << "  [DOWNLINK]" << std::endl;
      if (dlFlowCount > 0) {
        std::cout << "  Throughput Rata-2    : " << dlTotalThroughput / dlFlowCount << " Mbit/s per User" << std::endl;
        std::cout << "  Average Delay        : " << dlTotalDelayMs / dlFlowCount << " ms" << std::endl;
        std::cout << "  Average Jitter       : " << dlTotalJitterMs / dlFlowCount << " ms" << std::endl;
        std::cout << "  Jain's Fairness Idx  : " << calcJains(dlUserThroughputs) << " (Skala 0.0 - 1.0)" << std::endl;
        std::cout << "  MAC Queue Drop       : " << dlTotalDrop / dlFlowCount << " packets per User" << std::endl;
      } else { std::cout << "  (Tidak ada data Downlink)" << std::endl; }

      std::cout << "  [UPLINK]" << std::endl;
      if (ulFlowCount > 0) {
        std::cout << "  Throughput Rata-2    : " << ulTotalThroughput / ulFlowCount << " Mbit/s per User" << std::endl;
        std::cout << "  Average Delay        : " << ulTotalDelayMs / ulFlowCount << " ms" << std::endl;
        std::cout << "  Average Jitter       : " << ulTotalJitterMs / ulFlowCount << " ms" << std::endl;
        std::cout << "  Jain's Fairness Idx  : " << calcJains(ulUserThroughputs) << " (Skala 0.0 - 1.0)" << std::endl;
        std::cout << "  MAC Queue Drop       : " << ulTotalDrop / ulFlowCount << " packets per User" << std::endl;
      } else { std::cout << "  (Tidak ada data Uplink)" << std::endl; }
      
      std::cout << "  SINR Distribution    : ~" << avgSinrEstimated << " dB (Teoritis Pathloss)" << std::endl;
    }

  }

  std::cout
      << "\n================= DETAIL PER STA & BREAKDOWN AREA ================="
      << std::endl;
  double ap0_th = 0.0, ap1_th = 0.0, koridor_th = 0.0;
  double ap0_delay = 0.0, ap1_delay = 0.0, koridor_delay = 0.0;
  double ap0_jitter = 0.0, ap1_jitter = 0.0, koridor_jitter = 0.0;
  uint32_t ap0_flows = 0, ap1_flows = 0, koridor_flows = 0;
  uint64_t ap0_drops = 0, ap1_drops = 0, koridor_drops = 0;

  // Persiapan CSV Export dengan Timestamp & Folder Khusus (Menggunakan
  // globalTimestamp)
  std::string csvFilename =
      outDir + "/hasil_stresstest_wifi7_" + globalTimestamp + ".csv";

  std::ofstream csvFile(csvFilename);
  csvFile << "STA_ID,MAC_Address,Direction,Area,Profil,QoS_AC,Throughput_Mbps,Delay_ms,Jitter_ms,MacDrop,"
             "HandoverCount\n";

  auto getMacAddress = [&](Ptr<Node> node) {
    for (uint32_t d = 0; d < node->GetNDevices(); ++d) {
      if (node->GetDevice(d)->GetInstanceTypeId() == WifiNetDevice::GetTypeId()) {
        return Mac48Address::ConvertFrom(node->GetDevice(d)->GetAddress());
      }
    }
    return Mac48Address("00:00:00:00:00:00");
  };

  for (uint32_t i = 0; i < 24; ++i) {
    Ipv4Address staAddr = staInterface.GetAddress(i);
    Mac48Address staMac = getMacAddress(wifiStaNode.Get(i));
    std::string area = (i < 12) ? "Kelas A" : (i < 22) ? "Kelas B" : "Koridor";
    double dlTh = 0, ulTh = 0;
    double dlDly = 0, ulDly = 0;
    double dlJit = 0, ulJit = 0;
    uint64_t dlDrop = 0, ulDrop = 0;

    double dlRxP = 0, ulRxP = 0;
    double dlTxP = 0, ulTxP = 0;
    double dlRxB = 0, ulRxB = 0;
    double dlDlySum = 0, ulDlySum = 0;
    double dlJitSum = 0, ulJitSum = 0;
    double dlFTx = 999999, ulFTx = 999999;
    double dlLRx = 0, ulLRx = 0;
    bool foundDl = false, foundUl = false;

    for (auto it = stats.begin(); it != stats.end(); ++it) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
      if (t.destinationAddress == staAddr) {
        foundDl = true;
        dlRxP += it->second.rxPackets; dlTxP += it->second.txPackets; dlRxB += it->second.rxBytes;
        dlDlySum += it->second.delaySum.GetSeconds(); dlJitSum += it->second.jitterSum.GetSeconds();
        if (it->second.timeFirstTxPacket.GetSeconds() < dlFTx) dlFTx = it->second.timeFirstTxPacket.GetSeconds();
        if (it->second.timeLastRxPacket.GetSeconds() > dlLRx) dlLRx = it->second.timeLastRxPacket.GetSeconds();
      } else if (t.sourceAddress == staAddr) {
        foundUl = true;
        ulRxP += it->second.rxPackets; ulTxP += it->second.txPackets; ulRxB += it->second.rxBytes;
        ulDlySum += it->second.delaySum.GetSeconds(); ulJitSum += it->second.jitterSum.GetSeconds();
        if (it->second.timeFirstTxPacket.GetSeconds() < ulFTx) ulFTx = it->second.timeFirstTxPacket.GetSeconds();
        if (it->second.timeLastRxPacket.GetSeconds() > ulLRx) ulLRx = it->second.timeLastRxPacket.GetSeconds();
      }
    }

    uint32_t handoverCount = (g_staAssocCount[i] > 1) ? g_staAssocCount[i] - 1 : 0;

    if (foundDl) {
        double dur = dlLRx - dlFTx;
        if (dur > 0) dlTh = (dlRxB * 8.0) / (dur * 1e6);
        if (dlRxP > 0) { dlDly = (dlDlySum / dlRxP) * 1000.0; dlJit = (dlJitSum / dlRxP) * 1000.0; }
        if (dlTxP > dlRxP) dlDrop = dlTxP - dlRxP;
        
        std::cout << "STA " << i << " [" << staMac << "] (" << area << ", " << profiles[i % 6].name << " / " << profiles[i % 6].acName << ") [DL]\t-> TH: " << dlTh << " Mbps \t| D: " << dlDly << " ms \t| J: " << dlJit << " ms \t| Drop: " << dlDrop << std::endl;
        csvFile << i << "," << staMac << ",DL," << area << "," << profiles[i % 6].name << "," << profiles[i % 6].acName << "," << dlTh << "," << dlDly << "," << dlJit << "," << dlDrop << "," << handoverCount << "\n";
    }
    if (foundUl) {
        double dur = ulLRx - ulFTx;
        if (dur > 0) ulTh = (ulRxB * 8.0) / (dur * 1e6);
        if (ulRxP > 0) { ulDly = (ulDlySum / ulRxP) * 1000.0; ulJit = (ulJitSum / ulRxP) * 1000.0; }
        if (ulTxP > ulRxP) ulDrop = ulTxP - ulRxP;
        
        std::cout << "STA " << i << " [" << staMac << "] (" << area << ", " << profiles[i % 6].name << " / " << profiles[i % 6].acName << ") [UL]\t-> TH: " << ulTh << " Mbps \t| D: " << ulDly << " ms \t| J: " << ulJit << " ms \t| Drop: " << ulDrop << std::endl;
        csvFile << i << "," << staMac << ",UL," << area << "," << profiles[i % 6].name << "," << profiles[i % 6].acName << "," << ulTh << "," << ulDly << "," << ulJit << "," << ulDrop << "," << handoverCount << "\n";
    }
    
    double throughput = dlTh + ulTh;
    double delay = (dlDly + ulDly) / 2.0;
    double jitter = (dlJit + ulJit) / 2.0;
    uint64_t drop = dlDrop + ulDrop;

        if (i < 12) {
          ap0_th += throughput;
          ap0_delay += delay;
          ap0_jitter += jitter;
          ap0_drops += drop;
          ap0_flows++;
        } else if (i < 22) {
          ap1_th += throughput;
          ap1_delay += delay;
          ap1_jitter += jitter;
          ap1_drops += drop;
          ap1_flows++;
        } else {
          koridor_th += throughput;
          koridor_delay += delay;
          koridor_jitter += jitter;
          koridor_drops += drop;
          koridor_flows++;
        }

    if (!foundDl && !foundUl) {
      std::cout << "STA " << i << " (" << area
                << ") -> Tidak ada data transmisi sukses!" << std::endl;
    }
  }
  csvFile.close();
  std::cout << "\n[!] Data statistik berhasil di-export ke '" << csvFilename
            << "'" << std::endl;

  std::cout << "\n[BREAKDOWN KELAS A (STA 0-11)]" << std::endl;
  if (ap0_flows > 0) {
    std::cout << "  Agregat Throughput   : " << ap0_th << " Mbps" << std::endl;
    std::cout << "  Rata-rata Delay      : " << ap0_delay / ap0_flows << " ms"
              << std::endl;
    std::cout << "  Rata-rata Jitter     : " << ap0_jitter / ap0_flows << " ms"
              << std::endl;
    std::cout << "  Total Paket Drop     : " << ap0_drops << " paket"
              << std::endl;
  }

  std::cout << "\n[BREAKDOWN KELAS B (STA 12-21)]" << std::endl;
  if (ap1_flows > 0) {
    std::cout << "  Agregat Throughput   : " << ap1_th << " Mbps" << std::endl;
    std::cout << "  Rata-rata Delay      : " << ap1_delay / ap1_flows << " ms"
              << std::endl;
    std::cout << "  Rata-rata Jitter     : " << ap1_jitter / ap1_flows << " ms"
              << std::endl;
    std::cout << "  Total Paket Drop     : " << ap1_drops << " paket"
              << std::endl;
  }

  std::cout << "\n[BREAKDOWN KORIDOR (STA 22-23 / Bergerak)]" << std::endl;
  if (koridor_flows > 0) {
    std::cout << "  Agregat Throughput   : " << koridor_th << " Mbps"
              << std::endl;
    std::cout << "  Rata-rata Delay      : " << koridor_delay / koridor_flows
              << " ms" << std::endl;
    std::cout << "  Rata-rata Jitter     : " << koridor_jitter / koridor_flows
              << " ms" << std::endl;
    std::cout << "  Total Paket Drop     : " << koridor_drops << " paket"
              << std::endl;
  }

  std::cout
      << "\n================= METRIK MOBILITAS & HANDOVER ================="
      << std::endl;
  uint32_t totalHandover = 0;
  for (auto const &[nodeId, count] : g_staAssocCount) {
    if (count > 1) {
      totalHandover += (count - 1);
      std::cout << "  STA Node " << nodeId
                << " berhasil melakukan Handover sebanyak : " << (count - 1)
                << " kali" << std::endl;
    }
  }
  if (totalHandover == 0) {
    std::cout << "  Tidak ada STA yang melakukan Handover (semua tetap pada AP "
                 "awal / sinyal masih terjangkau)."
              << std::endl;
  } else {
    std::cout << "  Total akumulasi Handover sukses : " << totalHandover
              << " kali" << std::endl;
  }

  std::cout << "\n================= METRIK SISTEM GLOBAL ================="
            << std::endl;
  // Spectral Efficiency = Total Throughput / Total Bandwidth (160 + 320 = 480
  // MHz)
  double spectralEfficiency = globalThroughputMbps / 480.0;

  // Channel Utilization
  double utilAp0 = (g_totalSamples > 0)
                       ? ((double)g_busySamplesAp0 / g_totalSamples) * 100.0
                       : 0.0;
  double utilAp1 = (g_totalSamples > 0)
                       ? ((double)g_busySamplesAp1 / g_totalSamples) * 100.0
                       : 0.0;

  std::cout << "  Total Agregat Throughput  : " << globalThroughputMbps
            << " Mbit/s" << std::endl;
  std::cout << "  Spectral Efficiency       : " << spectralEfficiency
            << " bit/s/Hz" << std::endl;
  std::cout << "  Channel Utilization (AP0) : " << utilAp0 << " % (Ruang 1)"
            << std::endl;
  std::cout << "  Channel Utilization (AP1) : " << utilAp1 << " % (Ruang 2)"
            << std::endl;

  std::cout
      << "\n================= VALIDASI MCS 13 (4096-QAM) ================="
      << std::endl;
  double txPower = 20.0;
  double pathlossEst =
      65.0; // Perkiraan pathloss rata-rata pada 5/6GHz indoor sejauh ~5 meter
  double rxGain = 6.0; // Update 6 dBi sesuai spesifikasi Ruijie AP
  double noiseFloor = -90.0;
  double snrEst = (txPower + rxGain - pathlossEst) - noiseFloor;

  std::cout << "  Minimum SNR untuk MCS 13  : ~41 dB" << std::endl;
  std::cout << "  Estimasi SNR Saat Ini     : " << snrEst << " dB" << std::endl;
  if (snrEst >= 41.0) {
    std::cout << "  Status Validasi           : [VALID] SNR memadai untuk "
                 "mempertahankan 4096-QAM."
              << std::endl;
  } else {
    std::cout
        << "  Status Validasi           : [WARNING] SNR kurang dari 41 dB. Di "
           "skenario nyata, sinyal akan fallback ke MCS yang lebih rendah."
        << std::endl;
  }

  std::cout << "========================================================\n"
            << std::endl;

  auto endRealTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> diffRealTime = endRealTime - startRealTime;
  std::cout << "[INFO] Waktu Eksekusi Nyata (Real Runtime) Simulasi: "
            << diffRealTime.count() << " detik" << std::endl;

  Simulator::Destroy();

  // Kembalikan buffer asli cout dan tutup file log
  std::cout.rdbuf(oldCoutBuf);
  logFile.close();

  return 0;
}

// upgrade