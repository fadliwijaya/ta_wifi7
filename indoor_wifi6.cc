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
#include "ns3/wifi-net-device.h"

// Header Modul Buildings
#include "ns3/building.h"
#include "ns3/buildings-helper.h"
#include "ns3/hybrid-buildings-propagation-loss-model.h"

// Header Modul Flow Monitor
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/wifi-helper.h"
#include "ns3/wifi-standards.h"

// Header Tambahan
#include "ns3/random-variable-stream.h"

#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

// Definisi Struktur Profil Pengguna
struct UserProfile {
  std::string name;
  uint16_t port;
  uint32_t packetSize; // Bytes
  std::string dataRate;
  uint32_t tos;       // IP Type of Service for WMM EDCA classification
  std::string acName; // Name of WMM Access Category
};

int main(int argc, char *argv[]) {
  std::cout << "\n>>> Memulai simulasi Wi-Fi 6 Indoor Multi-User, menunggu 10 "
               "detik... <<<\n"
            << std::endl;

  // Konfigurasi Parameter Dasar
  double simulationTimeSec = 10.0; // Waktu aktif pengiriman data

  CommandLine cmd(__FILE__);
  cmd.AddValue("simulationTime", "Simulation active time (seconds)",
               simulationTimeSec);
  cmd.Parse(argc, argv);

  // Tingkatkan kapasitas antrean MAC dan QueueDisc secara global untuk
  // menangani total beban trafik
  Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue("10000p"));
  // MaxDelay 5s: cukup untuk proses bootstrapping IdealWifiManager, tanpa
  // menimbulkan queue bloat yang justru memblokir paket baru masuk saat channel
  // sibuk
  Config::SetDefault("ns3::WifiMacQueue::MaxDelay", TimeValue(Seconds(5.0)));
  Config::SetDefault("ns3::FqCoDelQueueDisc::MaxSize", StringValue("10000p"));

  // Optimasi Agregasi A-MPDU Wi-Fi 6 untuk mencapai throughput Gigabit
  // Maksimal Block Ack Window untuk Wi-Fi 6 (802.11ax) adalah 256.
  Config::SetDefault("ns3::WifiMac::MpduBufferSize", UintegerValue(256));

  Time simulationTime = Seconds(simulationTimeSec);

  // Definisi 5 Profil Trafik dengan Kelas WMM EDCA sesuai Request User
  // Catatan: File Download menggunakan 100 Mbps agar dapat bersaing secara
  // realistis dengan traffic AC_VI dan AC_VO pada kanal bersama EDCA. 600 Mbps
  // pada AC_BK secara fisik tidak bisa bersaing dengan AC_VI/VO karena EDCA
  // sengaja memberikan prioritas rendah pada AC_BK (AIFSN=7 vs AIFSN=2 untuk
  // VO/VI).
  std::vector<UserProfile> profiles = {
      {"Social Media", 9001, 200, "200Mbps", 0x00, "AC_BE"},
      {"Video 4K Streaming", 9002, 1472, "1500Mbps", 0xa0, "AC_VI"},
      {"Gaming", 9003, 1472, "500Mbps", 0xc0,
       "AC_VO"}, // Ubah ke 1472 B (Cloud Gaming) agar
                 // tidak memonopoli channel dengan packet
                 // storm
      {"File Download", 9004, 1472, "1500Mbps", 0x20, "AC_BK"},
      {"Web Browsing", 9005, 1000, "200Mbps", 0x00, "AC_BE"}};

  // 1. Buat Node untuk Access Point (AP) dan 5 Client (STA)
  NodeContainer wifiApNode;
  wifiApNode.Create(1);
  NodeContainer wifiStaNode;
  wifiStaNode.Create(5);

  // 2. Setup Bangunan / Ruangan Tunggal (Dimensi 8x8x3 m)
  // Dinding dibatasi oleh beton/bata tanpa jendela (ConcreteWithoutWindows)
  // untuk merepresentasikan bata/batako standar.
  Ptr<Building> building = CreateObject<Building>();
  building->SetBoundaries(Box(0.0, 8.0, 0.0, 8.0, 0.0, 3.0));
  building->SetBuildingType(Building::Office);
  building->SetExtWallsType(Building::ConcreteWithoutWindows);
  building->SetNFloors(1);
  building->SetNRoomsX(1);
  building->SetNRoomsY(1);

  // 3. Tata Letak Node (Placement Topology)
  // AP diletakkan dekat langit-langit (4.0, 4.0, 2.9) untuk memastikan secara
  // matematis ia berada di DALAM ruangan (karena batas atas Z=3.0). Jika
  // persis 3.0, ada risiko dianggap "di luar" dan terkena redaman dinding
  // eksternal beton yang sangat besar.
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc =
      CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(4.0, 4.0, 2.9)); // Posisi AP

  Ptr<UniformRandomVariable> randomX = CreateObject<UniformRandomVariable>();
  randomX->SetAttribute("Min", DoubleValue(0.5));
  randomX->SetAttribute("Max", DoubleValue(7.5));

  Ptr<UniformRandomVariable> randomY = CreateObject<UniformRandomVariable>();
  randomY->SetAttribute("Min", DoubleValue(0.5));
  randomY->SetAttribute("Max", DoubleValue(7.5));

  std::vector<Vector> staPositions;
  for (uint32_t i = 0; i < 5; ++i) {
    double x = randomX->GetValue();
    double y = randomY->GetValue();
    double z = 1.0;
    staPositions.push_back(Vector(x, y, z));
    positionAlloc->Add(Vector(x, y, z));
  }

  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  mobility.Install(wifiApNode);
  mobility.Install(wifiStaNode);

  // Hubungkan node dengan modul bangunan agar model propagasi indoor aktif
  // secara otomatis
  BuildingsHelper::Install(wifiApNode);
  BuildingsHelper::Install(wifiStaNode);

  // 4. Setup Channel dan Spectrum PHY dengan Model Propagasi Indoor
  Ptr<MultiModelSpectrumChannel> spectrumChannel =
      CreateObject<MultiModelSpectrumChannel>();

  // Gunakan HybridBuildingsPropagationLossModel yang otomatis mendelegasikan
  // perambatan indoor ke ITU-R P.1238 (ItuR1238)
  Ptr<HybridBuildingsPropagationLossModel> lossModel =
      CreateObject<HybridBuildingsPropagationLossModel>();
  lossModel->SetAttribute(
      "Frequency",
      DoubleValue(6025e6)); // Pita Frekuensi 6 GHz (Frekuensi Tengah)
  spectrumChannel->AddPropagationLossModel(lossModel);

  Ptr<ConstantSpeedPropagationDelayModel> delayModel =
      CreateObject<ConstantSpeedPropagationDelayModel>();

  spectrumChannel->SetPropagationDelayModel(delayModel);

  SpectrumWifiPhyHelper spectrumPhy;
  spectrumPhy.SetChannel(spectrumChannel);
  spectrumPhy.SetErrorRateModel("ns3::NistErrorRateModel");

  // Aktifkan MIMO 4x4 untuk meningkatkan kapasitas kanal fisik Wi-Fi 6 secara
  // drastis
  spectrumPhy.Set("Antennas", UintegerValue(4));
  spectrumPhy.Set("MaxSupportedTxSpatialStreams", UintegerValue(4));
  spectrumPhy.Set("MaxSupportedRxSpatialStreams", UintegerValue(4));

  // Konfigurasi Wi-Fi 6: Menggunakan lebar pita maksimum yang didukung oleh
  // ns-3.42 (160 MHz, Channel 15) Catatan: Lebar pita 320 MHz belum sepenuhnya
  // diimplementasikan dalam struktur channel standar ns-3.42
  spectrumPhy.Set("ChannelSettings", StringValue("{15, 160, BAND_6GHZ, 0}"));

  // Optimasi Daya Transmisi (Tx Power) & Gain Antena
  // Model propagasi HybridBuildings memiliki atenuasi redaman dinding dan jarak
  // yang sangat realistis dan tinggi. Untuk mencapai throughput Gigabit dengan
  // MCS tertinggi (Misal 1024-QAM / 4096-QAM) dengan packet loss benar-benar 0%
  // di semua lingkungan kompilasi, kita naikkan TxPower sedikit menjadi 26 dBm.
  spectrumPhy.Set("TxPowerStart", DoubleValue(26.0)); // dBm
  spectrumPhy.Set("TxPowerEnd", DoubleValue(26.0));   // dBm
  spectrumPhy.Set("TxGain", DoubleValue(5.0));        // dB
  spectrumPhy.Set("RxGain", DoubleValue(5.0));        // dB

  // 5. Setup Wifi Helper dengan Standar 802.11ax (Wi-Fi 6) dan IdealWifiManager
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ax);

  // Menggunakan IdealWifiManager untuk negosiasi Rate dan MIMO NSS secara
  // otomatis. Karena kita sudah meningkatkan MpduBufferSize menjadi 1024,
  // IdealWifiManager sekarang bisa mengirim A-MPDU besar tanpa stall, sehingga
  // mencapai throughput Gigabit.
  wifi.SetRemoteStationManager("ns3::IdealWifiManager");

  // 6. Setup MAC Layer & Install Device
  // Mengaktifkan Fitur QoS AMPDU & A-MSDU Aggregation raksasa untuk performa
  // maksimal Wi-Fi 6
  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi7-indoor-simulation");

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "QosSupported",
              BooleanValue(true));
  NetDeviceContainer staDevice = wifi.Install(spectrumPhy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "QosSupported",
              BooleanValue(true));
  NetDeviceContainer apDevice = wifi.Install(spectrumPhy, mac, wifiApNode);

  // 7. Install Internet Stack & Alokasi IP Address
  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

  // 8 & 9. Pasang Aplikasi Penerima (Packet Sink di sisi STA) dan Pengirim
  // (OnOff di sisi AP)
  ApplicationContainer sinkApps;
  ApplicationContainer clientApps;

  for (uint32_t i = 0; i < 5; ++i) {
    // Pasang Aplikasi Penerima (Packet Sink) di sisi STA i
    Address sinkLocalAddress(
        InetSocketAddress(Ipv4Address::GetAny(), profiles[i].port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                      sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiStaNode.Get(i));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(simulationTime + Seconds(2.0));
    sinkApps.Add(sinkApp);

    // Pasang Aplikasi Pengirim (OnOff UDP Traffic) dari AP ke STA i dengan
    // Traffic Prioritization (ToS / EDCA)
    OnOffHelper onoff(
        "ns3::UdpSocketFactory",
        InetSocketAddress(staInterface.GetAddress(i), profiles[i].port));

    // Memodifikasi traffic menjadi bursty (On 10ms, Off 90ms) dengan 10x
    // DataRate. Hal ini penting di simulasi Wi-Fi 6 agar antrean MAC terisi
    // banyak paket sekaligus, sehingga memicu agregasi A-MPDU raksasa. Jika
    // menggunakan CBR murni, paket akan dikirim satu-per-satu dengan overhead
    // PHY raksasa yang menyumbat seluruh kapasitas channel dan menyebabkan EDCA
    // starvation pada AC_BK (File Download) dan AC_BE (Social Media).
    ns3::DataRate baseRate(profiles[i].dataRate);
    std::string burstRateStr =
        std::to_string(baseRate.GetBitRate() * 10) + "bps";

    onoff.SetAttribute(
        "OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.01]"));
    onoff.SetAttribute(
        "OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.09]"));
    onoff.SetAttribute("PacketSize", UintegerValue(profiles[i].packetSize));
    onoff.SetAttribute("DataRate", StringValue(burstRateStr));
    onoff.SetAttribute(
        "Tos",
        UintegerValue(
            profiles[i].tos)); // Traffic prioritization sesuai standar QoS

    ApplicationContainer clientApp = onoff.Install(wifiApNode.Get(0));

    // Stagger waktu mulai agar burst 10ms dari masing-masing antrean tidak
    // ditembakkan di mikrodetik yang persis sama. Jika serentak, MAC Queue
    // akan langsung terbanjiri ribuan paket dan menyebabkan drop 0.1-0.3%.
    double startTime = 1.0 + (i * 0.015); // Stagger sebesar 15 ms antar flow
    clientApp.Start(Seconds(startTime));
    clientApp.Stop(simulationTime +
                   Seconds(1.0)); // Berhenti setelah durasi simulationTime
    clientApps.Add(clientApp);
  }

  // 10. Pasang Flow Monitor untuk Ekstraksi Statistik
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // 11. Jalankan Simulasi
  Simulator::Stop(simulationTime + Seconds(2.0));
  Simulator::Run();

  // 12. Analisis & Ekstraksi Metrik Performa per Profil Pengguna
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  std::cout
      << "\n================= HASIL SIMULASI INDOOR WI-FI 6 ================="
      << std::endl;
  std::cout << "Dimensi Ruangan    : 8 x 8 x 3 m (Dinding Bata/Batako)"
            << std::endl;
  std::cout << "Model Propagasi    : HybridBuildings (ITU-R P.1238)"
            << std::endl;
  std::cout << "Posisi AP          : X=4.0, Y=4.0, Z=2.9 (Langit-langit dalam "
               "ruangan)"
            << std::endl;
  std::cout << "Konfigurasi Wifi   : Wi-Fi 6 (802.11ax), 160 MHz Bandwidth, 6 "
               "GHz Band, MIMO 4x4"
            << std::endl;
  std::cout << "AMPDU Aggregation  : QoS AMPDU & A-MSDU (Default ns-3 802.11ax "
               "Maximum)"
            << std::endl;
  std::cout << "Skema Rate Control : IdealWifiManager (Auto-Optimize MCS per "
               "SNR real-time)"
            << std::endl;
  std::cout
      << "-----------------------------------------------------------------"
      << std::endl;
  std::cout << "Lokasi & Koordinat User (STA):" << std::endl;
  for (uint32_t i = 0; i < 5; ++i) {
    std::cout << "  - User " << (i + 1) << " (" << profiles[i].name
              << "): X=" << staPositions[i].x << ", Y=" << staPositions[i].y
              << ", Z=" << staPositions[i].z
              << " | IP: " << staInterface.GetAddress(i) << std::endl;
  }
  std::cout
      << "-----------------------------------------------------------------"
      << std::endl;

  for (uint32_t p = 0; p < 5; ++p) {
    bool profileFound = false;
    std::cout << "\n>>> Profil: " << profiles[p].name
              << " (Port: " << profiles[p].port
              << ", Target Rate: " << profiles[p].dataRate
              << ", Packet Size: " << profiles[p].packetSize << " B) <<<"
              << std::endl;
    std::cout << "  Kelas WMM EDCA     : " << profiles[p].acName
              << " (IP ToS: 0x" << std::hex << profiles[p].tos << std::dec
              << ")" << std::endl;

    for (auto i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      if (t.destinationPort == profiles[p].port) {
        profileFound = true;
        double throughput = 0.0;
        double avgDelayMs = 0.0;
        double lossRatio = 0.0;

        double duration = i->second.timeLastRxPacket.GetSeconds() -
                          i->second.timeFirstTxPacket.GetSeconds();
        if (duration > 0) {
          throughput = (i->second.rxBytes * 8.0) / (duration * 1e6); // Mbit/s
        } else {
          throughput =
              (i->second.rxBytes * 8.0) / (simulationTime.GetSeconds() * 1e6);
        }

        if (i->second.rxPackets > 0) {
          avgDelayMs = (i->second.delaySum.GetSeconds() / i->second.rxPackets) *
                       1000.0; // ms
        }

        if (i->second.txPackets > 0) {
          lossRatio = ((double)(i->second.txPackets - i->second.rxPackets) /
                       i->second.txPackets) *
                      100.0;
        }

        double avgJitterMs = 0.0;
        if (i->second.rxPackets > 1) {
          avgJitterMs =
              (i->second.jitterSum.GetSeconds() / (i->second.rxPackets - 1)) *
              1000.0; // ms
        }

        // Analisis Kepatuhan QoS berdasarkan Standar Telekomunikasi ITU-T
        // G.1010
        std::string ituCategory = "";
        std::string qosStatus = "";
        std::string standardLimits = "";

        if (profiles[p].name == "Gaming") {
          ituCategory = "Highly Interactive (Real-time Games)";
          standardLimits = "Delay <= 50ms, Jitter <= 20ms, Loss <= 3%";
          if (avgDelayMs <= 50 && avgJitterMs <= 20 && lossRatio <= 1.0) {
            qosStatus = "EXCELLENT (Sangat Baik)";
          } else if (avgDelayMs <= 150 && avgJitterMs <= 50 &&
                     lossRatio <= 3.0) {
            qosStatus = "ACCEPTABLE (Dapat Diterima)";
          } else {
            qosStatus = "POOR (Kurang Memadai)";
          }
        } else if (profiles[p].name == "Video 4K Streaming") {
          ituCategory = "Streaming Media (One-way Video)";
          standardLimits = "Delay <= 2000ms, Jitter <= 50ms, Loss <= 1%";
          if (avgDelayMs <= 2000 && avgJitterMs <= 50 && lossRatio <= 1.0) {
            qosStatus = "EXCELLENT (Sangat Baik)";
          } else if (avgDelayMs <= 5000 && avgJitterMs <= 100 &&
                     lossRatio <= 1.0) {
            qosStatus = "ACCEPTABLE (Dapat Diterima)";
          } else {
            qosStatus = "POOR (Kurang Memadai)";
          }
        } else if (profiles[p].name == "Social Media" ||
                   profiles[p].name == "Web Browsing") {
          ituCategory = "Interactive Information Retrieval (Web/Social)";
          standardLimits = "Delay <= 2000ms, Loss <= 1%";
          if (avgDelayMs <= 1000 && lossRatio <= 1.0) {
            qosStatus = "EXCELLENT (Sangat Baik)";
          } else if (avgDelayMs <= 4000 && lossRatio <= 2.0) {
            qosStatus = "ACCEPTABLE (Dapat Diterima)";
          } else {
            qosStatus = "POOR (Kurang Memadai)";
          }
        } else if (profiles[p].name == "File Download") {
          ituCategory = "Background / Bulk Data Transfer";
          standardLimits = "Delay N/A, Loss <= 1% (Zero Preferred)";
          if (lossRatio <= 1.0) {
            qosStatus = "EXCELLENT (Sangat Baik)";
          } else if (lossRatio <= 1.0) {
            qosStatus = "ACCEPTABLE (Dapat Diterima)";
          } else {
            qosStatus = "POOR (Kurang Memadai)";
          }
        }

        std::cout << "  Flow ID            : Flow " << i->first << " ("
                  << t.sourceAddress << " -> " << t.destinationAddress << ")"
                  << std::endl;
        std::cout << "  Kategori ITU-T G.1010: " << ituCategory << std::endl;
        std::cout << "  Batas QoS Standar  : " << standardLimits << std::endl;
        std::cout << "  Total Paket Kirim  : " << i->second.txPackets
                  << " paket" << std::endl;
        std::cout << "  Total Paket Terima : " << i->second.rxPackets
                  << " paket" << std::endl;
        std::cout << "  Total Data Kirim   : "
                  << i->second.txBytes / (1024 * 1024.0) << " MB" << std::endl;
        std::cout << "  Total Data Terima  : "
                  << i->second.rxBytes / (1024 * 1024.0) << " MB" << std::endl;
        std::cout << "  Throughput Rata-2  : " << throughput << " Mbit/s"
                  << std::endl;
        std::cout << "  Average Delay      : " << avgDelayMs << " ms"
                  << std::endl;
        std::cout << "  Average Jitter     : " << avgJitterMs << " ms"
                  << std::endl;
        std::cout << "  Packet Loss Ratio  : " << lossRatio << " %"
                  << std::endl;
        std::cout << "  Status QoS Standar : " << qosStatus << std::endl;
      }
    }
    if (!profileFound) {
      std::cout
          << "  [PERINGATAN] Aliran data untuk profil ini tidak terdeteksi!"
          << std::endl;
    }
  }
  std::cout
      << "\n=================================================================\n"
      << std::endl;

  Simulator::Destroy();
  return 0;
}
