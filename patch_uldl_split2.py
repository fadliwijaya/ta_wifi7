import os
import re

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

    if "Direction,Area,Profil" in content:
        continue

    # Update CSV Header
    content = re.sub(
        r'csvFile << "STA_ID,MAC_Address,Area,Profil,QoS_AC,Throughput_Mbps,Delay_ms,Jitter_ms,MacDrop,HandoverCount\\n";',
        'csvFile << "STA_ID,MAC_Address,Direction,Area,Profil,QoS_AC,Throughput_Mbps,Delay_ms,Jitter_ms,MacDrop,HandoverCount\\n";',
        content
    )

    # 1. Regex find the entire block from `double throughput = 0.0;` down to `csvFile << i << ...`
    pattern = re.compile(
        r'double throughput = 0\.0;.*?csvFile.*?HandoverCount.*?\\n";\n\s*\}', 
        re.DOTALL
    )

    new_block = """double dlTh = 0, ulTh = 0;
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
        
        totalThroughput += dlTh; totalDelay += dlDly; totalJitter += dlJit; totalDrop += dlDrop;
        std::cout << "STA " << i << " [" << staMac << "] (" << area << ", " << profiles[i % 5].name << " / " << profiles[i % 5].acName << ") [DL]\\t-> TH: " << dlTh << " Mbps \\t| D: " << dlDly << " ms \\t| J: " << dlJit << " ms \\t| Drop: " << dlDrop << std::endl;
        csvFile << i << "," << staMac << ",DL," << area << "," << profiles[i % 5].name << "," << profiles[i % 5].acName << "," << dlTh << "," << dlDly << "," << dlJit << "," << dlDrop << "," << handoverCount << "\\n";
    }
    if (foundUl) {
        double dur = ulLRx - ulFTx;
        if (dur > 0) ulTh = (ulRxB * 8.0) / (dur * 1e6);
        if (ulRxP > 0) { ulDly = (ulDlySum / ulRxP) * 1000.0; ulJit = (ulJitSum / ulRxP) * 1000.0; }
        if (ulTxP > ulRxP) ulDrop = ulTxP - ulRxP;
        
        totalThroughput += ulTh; totalDelay += ulDly; totalJitter += ulJit; totalDrop += ulDrop;
        std::cout << "STA " << i << " [" << staMac << "] (" << area << ", " << profiles[i % 5].name << " / " << profiles[i % 5].acName << ") [UL]\\t-> TH: " << ulTh << " Mbps \\t| D: " << ulDly << " ms \\t| J: " << ulJit << " ms \\t| Drop: " << ulDrop << std::endl;
        csvFile << i << "," << staMac << ",UL," << area << "," << profiles[i % 5].name << "," << profiles[i % 5].acName << "," << ulTh << "," << ulDly << "," << ulJit << "," << ulDrop << "," << handoverCount << "\\n";
    }"""

    if pattern.search(content):
        content = pattern.sub(new_block, content)
        with open(filepath, "w") as file:
            file.write(content)
        print(f"Patched regex in {f}")
    else:
        print(f"Regex failed in {f}")

