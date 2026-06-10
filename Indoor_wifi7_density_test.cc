#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/buildings-module.h"
#include "ns3/csma-module.h"
#include <iostream>
#include <fstream>
#include <iomanip>

using namespace ns3;

uint64_t g_prevRxPackets = 0;
uint64_t g_prevTxPackets = 0;
double g_prevTotalDelay = 0.0;
double g_prevTotalJitter = 0.0;
uint32_t g_prevTotalDrops = 0;

std::ofstream g_csvFile;
Ptr<FlowMonitor> g_flowMonitor;

void MonitorQoS(Time stepTime, uint32_t totalStas) {
    g_flowMonitor->CheckForLostPackets();
    std::map<FlowId, FlowMonitor::FlowStats> stats = g_flowMonitor->GetFlowStats();

    uint64_t currentRxPackets = 0;
    uint64_t currentTxPackets = 0;
    double totalDelay = 0;
    double totalJitter = 0;
    uint32_t totalDrops = 0;

    for (auto const& stat : stats) {
        currentRxPackets += stat.second.rxPackets;
        currentTxPackets += stat.second.txPackets;
        totalDelay += stat.second.delaySum.GetSeconds();
        totalJitter += stat.second.jitterSum.GetSeconds();
        totalDrops += (stat.second.txPackets - stat.second.rxPackets);
    }

    uint64_t deltaRx = currentRxPackets - g_prevRxPackets;
    double deltaDelay = totalDelay - g_prevTotalDelay;
    double deltaJitter = totalJitter - g_prevTotalJitter;
    uint32_t deltaDrop = totalDrops - g_prevTotalDrops;
    
    double throughputMbps = 0.0;
    if (deltaRx > 0) {
       throughputMbps = (deltaRx * 1472 * 8.0) / (stepTime.GetSeconds() * 1e6);
    }

    double instDelayMs = deltaRx > 0 ? (deltaDelay / deltaRx) * 1000.0 : 0.0;
    double instJitterMs = deltaRx > 0 ? (deltaJitter / deltaRx) * 1000.0 : 0.0;

    double now = Simulator::Now().GetSeconds();
    uint32_t activeStas = 0;
    if (now >= 10.0 && now < 15.0) activeStas = 10;
    else if (now >= 15.0 && now < 20.0) activeStas = 20;
    else if (now >= 20.0 && now < 25.0) activeStas = 30;
    else if (now >= 25.0 && now < 30.0) activeStas = 40;
    else if (now >= 30.0) activeStas = 50;

    std::cout << "[INFO] Waktu: " << now << "s | Active STAs: " << activeStas
              << " | Tput: " << throughputMbps << " Mbps"
              << " | Delay: " << instDelayMs << " ms"
              << " | Jitter: " << instJitterMs << " ms"
              << " | Drop: " << deltaDrop << "\n";
              
    g_csvFile << now << "," << activeStas << "," << throughputMbps << "," << instDelayMs << "," << instJitterMs << "," << deltaDrop << "\n";

    g_prevRxPackets = currentRxPackets;
    g_prevTxPackets = currentTxPackets;
    g_prevTotalDelay = totalDelay;
    g_prevTotalJitter = totalJitter;
    g_prevTotalDrops = totalDrops;

    Simulator::Schedule(stepTime, &MonitorQoS, stepTime, totalStas);
}

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    uint32_t nStas = 50;

    std::string timeStamp = "20260609_200000"; 
    std::string logFileName = "scratch/ta_wifi7/output_simulasi/terminal_output_wifi7_density_" + timeStamp + ".log";
    std::string csvFileName = "scratch/ta_wifi7/output_simulasi/hasil_density_wifi7_" + timeStamp + ".csv";
    
    // Redirect stdout to file (TeeStream logic)
    // We will just output to terminal, user can > file if needed. But let's use standard cout and also write to CSV.
    g_csvFile.open(csvFileName);
    g_csvFile << "Waktu(s),Active_STAs,Throughput(Mbps),Delay(ms),Jitter(ms),PacketDrop\n";

    std::cout << "=======================================================\n";
    std::cout << "[INFO] Skenario High-Density / Kepadatan User (Wi-Fi 7)\n";
    std::cout << "=======================================================\n";
    std::cout << "Simulasi ini dirancang untuk menguji ketahanan jaringan Wi-Fi terhadap kepadatan pengguna yang ekstrim (high-density test). Tujuannya adalah untuk mengamati seberapa baik jaringan dapat bertahan — dari sisi throughput total, keterlambatan (delay), hingga tingkat kehilangan paket (packet loss) — ketika jumlah perangkat klien terus bertambah seiring berjalannya waktu.\n\n";
    std::cout << "Secara konseptual, simulasi ini menggunakan sebuah Access Point (AP) utama yang memancarkan sinyal ke area ruangan. Pada detik-detik awal, hanya ada sedikit pengguna yang terkoneksi. Namun setiap beberapa detik, gelombang pengguna baru akan datang dan langsung terhubung, meminta alokasi bandwidth yang besar (150 Mbps, setara streaming Video 8K/VR) secara agresif.\n\n";
    std::cout << "Dengan pendekatan penambahan perangkat secara bertahap ini, kita dapat melihat titik jenuh jaringan di mana Access Point mulai kewalahan memproses antrean paket data, sehingga memberikan gambaran nyata tentang kapasitas maksimum jumlah pengguna simultan (user concurrency) yang dapat ditangani oleh Access Point Wi-Fi 7.\n";
    std::cout << "=======================================================\n";
    std::cout << "[SPESIFIKASI SIMULASI]\n";
    std::cout << "- Standar Wi-Fi     : Wi-Fi 7 (802.11be)\n";
    std::cout << "- Konfigurasi MIMO  : 8x8 Spatial Streams\n";
    std::cout << "- Fitur Spesial     : Multi-Link Operation (MLO) Aktif\n";
    std::cout << "    * Link 0        : Frekuensi 5 GHz (Lebar Pita 160 MHz)\n";
    std::cout << "    * Link 1        : Frekuensi 6 GHz (Lebar Pita 320 MHz)\n";
    std::cout << "- Tx Power          : 20.0 dBm\n";
    std::cout << "- Rate Adaptation   : IdealWifiManager (Hingga 4096-QAM)\n";
    std::cout << "- Total User (STA)  : " << nStas << " Perangkat\n";
    std::cout << "- Beban per User    : 150 Mbps (Video 8K / VR Streaming)\n";
    std::cout << "=======================================================\n\n";

    Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue("5000p"));
    Config::SetDefault("ns3::WifiMacQueue::MaxDelay", TimeValue(Seconds(1.0)));
    Config::SetDefault("ns3::FqCoDelQueueDisc::MaxSize", StringValue("5000p"));

    Config::SetDefault("ns3::WifiMac::MpduBufferSize", UintegerValue(1024));
    Config::SetDefault("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue(1048575));
    Config::SetDefault("ns3::WifiMac::BK_MaxAmpduSize", UintegerValue(1048575));
    Config::SetDefault("ns3::WifiMac::VI_MaxAmpduSize", UintegerValue(1048575));
    Config::SetDefault("ns3::WifiMac::VO_MaxAmpduSize", UintegerValue(1048575));
    Config::SetDefault("ns3::StaWifiMac::MaxMissedBeacons", UintegerValue(100));

    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStas);

    Ptr<Building> building = CreateObject<Building>();
    building->SetBoundaries(Box(0.0, 16.0, 0.0, 11.0, 0.0, 3.0));
    building->SetBuildingType(Building::Office);
    building->SetExtWallsType(Building::ConcreteWithoutWindows);
    building->SetNFloors(1);
    building->SetNRoomsX(1);
    building->SetNRoomsY(1);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> apPosAlloc = CreateObject<ListPositionAllocator>();
    apPosAlloc->Add(Vector(8.0, 5.5, 2.9)); 
    mobility.SetPositionAllocator(apPosAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(1.0),
                                  "MinY", DoubleValue(1.0),
                                  "DeltaX", DoubleValue(1.5),
                                  "DeltaY", DoubleValue(1.5),
                                  "GridWidth", UintegerValue(10), // 10x5 grid
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(wifiStaNodes);

    BuildingsHelper::Install(wifiApNode);
    BuildingsHelper::Install(wifiStaNodes);

    SpectrumWifiPhyHelper spectrumPhy(2); // MLO
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
    wifi.SetObssPdAlgorithm("ns3::ConstantObssPdAlgorithm", "ObssPdLevel", DoubleValue(-82.0));

    WifiMacHelper mac;
    Ssid ssid = Ssid("HighDensity-WiFi7");
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(true));
    NetDeviceContainer apDevice = wifi.Install(spectrumPhy, mac, wifiApNode);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(true), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(spectrumPhy, mac, wifiStaNodes);

    for (uint32_t i = 0; i < apDevice.GetN(); ++i) {
        DynamicCast<WifiNetDevice>(apDevice.Get(i))->GetPhy()->SetMaxSupportedTxSpatialStreams(8);
        DynamicCast<WifiNetDevice>(apDevice.Get(i))->GetPhy()->SetMaxSupportedRxSpatialStreams(8);
    }
    for (uint32_t i = 0; i < staDevices.GetN(); ++i) {
        DynamicCast<WifiNetDevice>(staDevices.Get(i))->GetPhy()->SetMaxSupportedTxSpatialStreams(8);
        DynamicCast<WifiNetDevice>(staDevices.Get(i))->GetPhy()->SetMaxSupportedRxSpatialStreams(8);
    }

    NodeContainer serverNode;
    serverNode.Create(1);

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
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer backboneInterfaces = address.Assign(backboneDevices);

    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;
    
    // Install PacketSink on all STAs
    for (uint32_t i = 0; i < nStas; ++i) {
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = sink.Install(wifiStaNodes.Get(i));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(40.0));
    }

    // Install OnOff sources on AP, pointing to each STA
    for (uint32_t i = 0; i < nStas; ++i) {
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(i), port));
        onoff.SetAttribute("DataRate", StringValue("150Mbps")); // 150 Mbps per user
        onoff.SetAttribute("PacketSize", UintegerValue(1472));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("Tos", UintegerValue(0xa0)); // QoS AC_VI (Video/VR Streaming)
        
        ApplicationContainer sourceApps = onoff.Install(serverNode.Get(0));
        
        // Staggered Start times for dynamic load and ARP resolution
        double startTime = 10.0 + (i * 0.02);
        if (i >= 10 && i < 20) startTime = 15.0 + ((i-10) * 0.02);
        else if (i >= 20 && i < 30) startTime = 20.0 + ((i-20) * 0.02);
        else if (i >= 30 && i < 40) startTime = 25.0 + ((i-30) * 0.02);
        else if (i >= 40) startTime = 30.0 + ((i-40) * 0.02);

        sourceApps.Start(Seconds(startTime));
        sourceApps.Stop(Seconds(40.0));
    }

    FlowMonitorHelper flowmon;
    g_flowMonitor = flowmon.InstallAll();

    Simulator::Schedule(Seconds(10.0), &MonitorQoS, Seconds(1.0), nStas);

    Simulator::Stop(Seconds(40.0));
    Simulator::Run();

    g_csvFile.close();
    Simulator::Destroy();
    return 0;
}
