#include "ns3/command-line.h"
#include "ns3/config.h"
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
#include "ns3/wifi-net-device.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    std::cout << "\n>>> Memulai simulasi, menunggu 30 detik... <<<\n" << std::endl;
    


    double distance = 5; // Jarak dalam meter
    Time simulationTime = Seconds(30); // DIUBAH: dari 5 detik menjadi 30 detik
    uint32_t payloadSize = 1472;         // Bytes (UDP)
    std::string dataRateBE = "EhtMcs13"; // Modulasi tertinggi Wi-Fi 7 (4096-QAM)

    CommandLine cmd(__FILE__);
    cmd.AddValue("distance", "Distance between AP and STA", distance);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.Parse(argc, argv);

    // Cetak pesan awal sesuai permintaan
    std::cout << "\n>>> Memulai simulasi, menunggu " << simulationTime.GetSeconds() << " detik... <<<\n" << std::endl;

    // 1. Buat Node untuk AP dan Client (STA)
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);

    // 2. Setup Channel dan Spectrum PHY untuk Wi-Fi 7
    Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();

    Ptr<FriisPropagationLossModel> lossModel = CreateObject<FriisPropagationLossModel>();
    lossModel->SetFrequency(6025e6); // Frekuensi tengah Band 6 GHz
    spectrumChannel->AddPropagationLossModel(lossModel);

    Ptr<ConstantSpeedPropagationDelayModel> delayModel =
        CreateObject<ConstantSpeedPropagationDelayModel>();
    spectrumChannel->SetPropagationDelayModel(delayModel);

    SpectrumWifiPhyHelper spectrumPhy;
    spectrumPhy.SetChannel(spectrumChannel);
    spectrumPhy.SetErrorRateModel("ns3::NistErrorRateModel");

    // Konfigurasi Wi-Fi 7: Channel 15 pada pita 6 GHz dengan lebar 160 MHz
    spectrumPhy.Set("ChannelSettings", StringValue("{15, 160, BAND_6GHZ, 0}"));

    // 3. Setup Wifi Helper dengan Standar 802.11be (Wi-Fi 7)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211be);

    // Menggunakan ConstantRate agar kita bisa mengunci MCS ke EhtMcs15 (4096-QAM)
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue(dataRateBE),
                                 "ControlMode",
                                 StringValue("EhtMcs0"));

    // 4. Setup MAC Layer & Install Device
    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-7-simulation");

    // Install di Client
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevice = wifi.Install(spectrumPhy, mac, wifiStaNode);

    // Install di AP
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(spectrumPhy, mac, wifiApNode);

    // 5. Setup Posisi (Mobility Model)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));      // Posisi AP
    positionAlloc->Add(Vector(distance, 0.0, 0.0)); // Posisi STA (berjarak 'distance' meter)
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNode);

    // 6. Install Internet Stack & Alokasi IP
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // 7. Pasang Aplikasi Penampung Data (Packet Sink di sisi STA)
    uint16_t port = 9;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiStaNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(simulationTime + Seconds(1.0));

    // 8. Pasang Aplikasi Pengirim Data (On-Off Traffic dari AP ke STA)
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(staInterface.GetAddress(0), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoff.SetAttribute(
        "DataRate",
        StringValue("2000Mbps")); // Set target data rate tinggi untuk menguji Wi-Fi 7

    ApplicationContainer clientApp = onoff.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(simulationTime + Seconds(1.0));

    // 9. Jalankan Simulasi
    Simulator::Stop(simulationTime + Seconds(1.0));
    Simulator::Run();

    // 10. Hitung Output/Metrik Throughput
    double totalBytesRx = DynamicCast<PacketSink>(sinkApp.Get(0))->GetTotalRx();
    double throughput =
        (totalBytesRx * 8) / (simulationTime.GetMicroSeconds()); // Hasil dalam Mbit/s

    std::cout << "\n================ SIMULASI SELESAI ================" << std::endl;
    std::cout << "Jarak AP - STA : " << distance << " meter" << std::endl;
    std::cout << "Modulasi Utama : " << dataRateBE << " (4096-QAM)" << std::endl;
    std::cout << "Total Data Diterima: " << totalBytesRx / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Throughput     : " << throughput << " Mbit/s" << std::endl;
    std::cout << "==================================================\n" << std::endl;

    Simulator::Destroy();
    return 0;
}