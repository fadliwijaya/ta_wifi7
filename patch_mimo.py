import os, glob, re

for file in glob.glob("*.cc"):
    with open(file, "r") as f:
        content = f.read()
        
    if "spectrumPhy.Set(\"MaxSupportedTxSpatialStreams\"" not in content and "roguePhy.Set(\"MaxSupportedTxSpatialStreams\"" not in content:
        continue

    print(f"Patching {file}...")

    # Strip the Set commands
    content = re.sub(r'^\s*spectrumPhy\.Set\("MaxSupported(?:Tx|Rx)SpatialStreams".*\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*roguePhy\.Set\("MaxSupported(?:Tx|Rx)SpatialStreams".*\n', '', content, flags=re.MULTILINE)

    # Find all containers created with wifi.Install(spectrumPhy
    spec_containers = re.findall(r'NetDeviceContainer\s+(\w+)\s*=\s*wifi\.Install\(spectrumPhy', content)
    # Find all containers created with wifi.Install(roguePhy
    rogue_containers = re.findall(r'NetDeviceContainer\s+(\w+)\s*=\s*wifi\.Install\(roguePhy', content)
    
    inject_str = "\n  // [AUTO-PATCH] Bypassing HT_PHY crash by setting MIMO after interface installation\n"
    for c in set(spec_containers):
        inject_str += f"""  for (uint32_t i = 0; i < {c}.GetN(); ++i) {{
      ns3::DynamicCast<ns3::WifiNetDevice>({c}.Get(i))->GetPhy()->SetMaxSupportedTxSpatialStreams(8);
      ns3::DynamicCast<ns3::WifiNetDevice>({c}.Get(i))->GetPhy()->SetMaxSupportedRxSpatialStreams(8);
  }}\n"""
    
    for c in set(rogue_containers):
        inject_str += f"""  for (uint32_t i = 0; i < {c}.GetN(); ++i) {{
      ns3::DynamicCast<ns3::WifiNetDevice>({c}.Get(i))->GetPhy()->SetMaxSupportedTxSpatialStreams(2);
      ns3::DynamicCast<ns3::WifiNetDevice>({c}.Get(i))->GetPhy()->SetMaxSupportedRxSpatialStreams(2);
  }}\n"""

    if "InternetStackHelper stack;" in content:
        content = content.replace("InternetStackHelper stack;", inject_str + "\n  InternetStackHelper stack;")
    elif "CsmaHelper csma;" in content:
        content = content.replace("CsmaHelper csma;", inject_str + "\n  CsmaHelper csma;")
    elif "FlowMonitorHelper flowmon;" in content:
        content = content.replace("FlowMonitorHelper flowmon;", inject_str + "\n  FlowMonitorHelper flowmon;")
        
    with open(file, "w") as f:
        f.write(content)

print("Patching complete!")
