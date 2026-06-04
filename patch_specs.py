import os

files = [
    "Indoor_wifi6_baseline.cc",
    "Indoor_wifi6_stress_test.cc",
    "Indoor_wifi7_final_upgrade.cc",
    "Indoor_wifi7_stress_test.cc",
    "Indoor_wifi7_pcap.cc"
]

for f in files:
    filepath = os.path.join("scratch/ta_wifi7", f)
    with open(filepath, "r") as file:
        content = file.read()
    
    is_wifi7 = "wifi7" in f.lower()

    if is_wifi7:
        specs = """  std::cout << "================= SPESIFIKASI PARAMETER SIMULASI =================" << std::endl;
  std::cout << "Standar Wi-Fi      : IEEE 802.11be (Wi-Fi 7)" << std::endl;
  std::cout << "Lebar Pita (BW)    : 320 MHz (Multi-Link 5 GHz & 6 GHz)" << std::endl;
  std::cout << "Max Modulasi       : MCS 13 (4096-QAM)" << std::endl;
  std::cout << "Antena / MIMO      : 2x2 Spatial Streams" << std::endl;
  std::cout << "Tx Power           : 20 dBm" << std::endl;
  std::cout << "Routing / QoS      : IPv4 Static Routing / WMM (EDCA)" << std::endl;
  std::cout << "Propagation Model  : HybridBuildingsPropagationLossModel" << std::endl;"""
    else:
        specs = """  std::cout << "================= SPESIFIKASI PARAMETER SIMULASI =================" << std::endl;
  std::cout << "Standar Wi-Fi      : IEEE 802.11ax (Wi-Fi 6)" << std::endl;
  std::cout << "Lebar Pita (BW)    : 160 MHz (Single-Link 5 GHz)" << std::endl;
  std::cout << "Max Modulasi       : MCS 11 (1024-QAM)" << std::endl;
  std::cout << "Antena / MIMO      : 2x2 Spatial Streams" << std::endl;
  std::cout << "Tx. Power           : 20 dBm" << std::endl;
  std::cout << "Routing / QoS      : IPv4 Static Routing / WMM (EDCA)" << std::endl;
  std::cout << "Propagation Model  : HybridBuildingsPropagationLossModel" << std::endl;"""
    
    # We will inject this right after "24 User" print statement
    target_str = 'std::cout << "Jumlah User (STA)  : 24 User (12 Kelas A, 10 Kelas B, 2 Koridor)"\n            << std::endl;'
    if target_str in content and "SPESIFIKASI PARAMETER SIMULASI" not in content:
        content = content.replace(target_str, target_str + "\n" + specs)

    with open(filepath, "w") as file:
        file.write(content)
    print(f"Patched specs in {filepath}")

