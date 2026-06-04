import os

files = [
    "Indoor_wifi6_baseline.cc",
    "Indoor_wifi6_stress_test.cc",
    "Indoor_wifi7_final_upgrade.cc",
    "Indoor_wifi7_stress_test.cc",
    "Indoor_wifi7_pcap.cc"
]

injection = """  staticRoutingAp1->SetDefaultRoute(backboneInterfaces.GetAddress(0), 1);

  // STA Routing: Default route ke AP masing-masing agar bisa kirim Uplink ke Server
  for (uint32_t i = 0; i < 10; ++i) {
    Ptr<Ipv4> ipv4Sta = wifiStaNode.Get(i)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRoutingSta = ipv4RoutingHelper.GetStaticRouting(ipv4Sta);
    staticRoutingSta->SetDefaultRoute(ap0Interface.GetAddress(0), 1);
  }
  for (uint32_t i = 10; i < 20; ++i) {
    Ptr<Ipv4> ipv4Sta = wifiStaNode.Get(i)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRoutingSta = ipv4RoutingHelper.GetStaticRouting(ipv4Sta);
    staticRoutingSta->SetDefaultRoute(ap1Interface.GetAddress(0), 1);
  }"""

for f in files:
    filepath = os.path.join("scratch/ta_wifi7", f)
    with open(filepath, "r") as file:
        content = file.read()
        
    if "STA Routing: Default route" in content:
        continue
        
    content = content.replace("  staticRoutingAp1->SetDefaultRoute(backboneInterfaces.GetAddress(0), 1);", injection)
    
    with open(filepath, "w") as file:
        file.write(content)
        
    print(f"Patched routing in {f}")
