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

    # Change the loop bound from 5 to 6
    content = content.replace("for (uint32_t p = 0; p < 5; ++p) {", "for (uint32_t p = 0; p < 6; ++p) {")

    # Replace the single profile summary with DL/UL profile summary
    old_summary_block_pattern = r'(\s*double totalThroughput = 0\.0;.*?)\s*if \(profileFound && flowCount > 0\) \{.*?std::cout << "  SINR Distribution    : ~" << avgSinrEstimated\s*<< " dB \(Teoritis Pathloss\)" << std::endl;\s*\}'
    
    new_summary_block = """
    double dlTotalThroughput = 0.0, ulTotalThroughput = 0.0;
    double dlTotalDelayMs = 0.0, ulTotalDelayMs = 0.0;
    double dlTotalJitterMs = 0.0, ulTotalJitterMs = 0.0;
    uint64_t dlTotalDrop = 0, ulTotalDrop = 0;
    int dlFlowCount = 0, ulFlowCount = 0;

    std::vector<double> dlUserThroughputs, ulUserThroughputs;

    for (uint32_t i = 0; i < 24; ++i) {
      if (i % 6 != p) continue; // Hanya hitung STA yang menggunakan profil ini

      Ipv4Address staAddr = wifiStaInterfaces.GetAddress(i);
      
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
"""
    content = re.sub(old_summary_block_pattern, new_summary_block, content, flags=re.DOTALL)
    
    # Also update the title of the profile to show DL/UL targets
    content = content.replace('std::cout << "\\n>>> Profil: " << profiles[p].name\n              << " (Target: " << profiles[p].dataRate << ") <<<" << std::endl;', 'std::cout << "\\n>>> Profil: " << profiles[p].name\n              << " (Target DL: " << profiles[p].dataRate << " | UL: " << profiles[p].ulDataRate << ") <<<" << std::endl;')

    # Fix globalThroughputMbps
    # Actually wait, globalThroughputMbps is now added twice because later in the STA loop we also have totalThroughput += dlTh + ulTh;
    # No, wait. We need to make sure we don't double count.
    # The original code didn't double count because the original STA loop did not add to globalThroughputMbps!
    # Let me check if globalThroughputMbps is added in the STA loop.
    
    with open(filepath, "w") as file:
        file.write(content)
    print(f"Patched {filepath}")

