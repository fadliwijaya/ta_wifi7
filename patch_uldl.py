import os
import re

files = [
    "Indoor_wifi6_baseline.cc",
    "Indoor_wifi6_stress_test.cc",
    "Indoor_wifi7_final_upgrade.cc",
    "Indoor_wifi7_stress_test.cc"
]

for f in files:
    filepath = os.path.join("scratch/ta_wifi7", f)
    with open(filepath, "r") as file:
        content = file.read()
        
    # Check if already patched
    if "INJEKSI TRAFIK UPLINK" in content:
        print(f"Skipping {f}, already patched.")
        continue

    # 1. Patch the Traffic Generation
    ul_injection = """    clientApps.Add(clientApp);

    // ==========================================
    // INJEKSI TRAFIK UPLINK (STA -> Server)
    // ==========================================
    uint16_t ulPort = profiles[profileIdx].port + 1000;
    
    PacketSinkHelper packetSinkUl("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), ulPort));
    ApplicationContainer sinkAppUl = packetSinkUl.Install(serverNode.Get(0));
    sinkAppUl.Start(Seconds(0.0));
    sinkAppUl.Stop(simulationTime + Seconds(2.0));

    OnOffHelper onoffUl("ns3::UdpSocketFactory", InetSocketAddress(backboneInterfaces.GetAddress(0), ulPort));
    onoffUl.SetAttribute("PacketSize", UintegerValue(profiles[profileIdx].packetSize));
    onoffUl.SetAttribute("TypeOfService", UintegerValue(profiles[profileIdx].tos));
"""
    if "stress_test" in f:
        ul_injection += """    onoffUl.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoffUl.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    std::string ulRateStr = std::to_string(baseRate.GetBitRate()) + "bps"; // 1x base rate for UL (since DL is 2x)
    onoffUl.SetAttribute("DataRate", StringValue(ulRateStr));
"""
    else:
        ul_injection += """    onoffUl.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.01]"));
    onoffUl.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.09]"));
    std::string ulRateStr = std::to_string(baseRate.GetBitRate() / 2) + "bps"; // 50% of base rate
    onoffUl.SetAttribute("DataRate", StringValue(ulRateStr));
"""

    ul_injection += """
    ApplicationContainer clientAppUl = onoffUl.Install(wifiStaNode.Get(i));
    clientAppUl.Start(Seconds(startTime + 0.05)); // delay 50ms agar tidak collision sempurna
    clientAppUl.Stop(simulationTime + Seconds(1.0));
    clientApps.Add(clientAppUl);"""
    
    content = content.replace("    clientApps.Add(clientApp);", ul_injection)

    # 2. Patch the FlowMonitor stats parsing
    old_flow_block = """    for (auto it = stats.begin(); it != stats.end(); ++it) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
      if (t.destinationAddress == staAddr) {
        double throughput = 0.0;
        double duration = it->second.timeLastRxPacket.GetSeconds() -
                          it->second.timeFirstTxPacket.GetSeconds();
        if (duration > 0)
          throughput = (it->second.rxBytes * 8.0) / (duration * 1e6);

        double delay = 0.0;
        if (it->second.rxPackets > 0)
          delay = (it->second.delaySum.GetSeconds() / it->second.rxPackets) *
                  1000.0;

        double jitter = 0.0;
        if (it->second.rxPackets > 0)
          jitter = (it->second.jitterSum.GetSeconds() / it->second.rxPackets) *
                   1000.0;

        uint64_t drop = 0;
        if (it->second.txPackets > it->second.rxPackets) {
          drop = it->second.txPackets - it->second.rxPackets;
        }

        // Simpan untuk agregat
        totalThroughput += throughput;
        totalDelay += delay;
        totalJitter += jitter;
        totalDrop += drop;"""

    new_flow_block = """    double throughput = 0.0;
    double delay = 0.0;
    double jitter = 0.0;
    uint64_t drop = 0;

    double rxPackets = 0;
    double txPackets = 0;
    double rxBytes = 0;
    double delaySum = 0;
    double jitterSum = 0;
    double firstTx = 999999.0;
    double lastRx = 0.0;
    bool found = false;

    for (auto it = stats.begin(); it != stats.end(); ++it) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
      // Aggregate UL and DL flows
      if (t.destinationAddress == staAddr || t.sourceAddress == staAddr) {
        found = true;
        rxPackets += it->second.rxPackets;
        txPackets += it->second.txPackets;
        rxBytes += it->second.rxBytes;
        delaySum += it->second.delaySum.GetSeconds();
        jitterSum += it->second.jitterSum.GetSeconds();
        
        if (it->second.timeFirstTxPacket.GetSeconds() < firstTx) {
            firstTx = it->second.timeFirstTxPacket.GetSeconds();
        }
        if (it->second.timeLastRxPacket.GetSeconds() > lastRx) {
            lastRx = it->second.timeLastRxPacket.GetSeconds();
        }
      }
    }

    if (found) {
        double duration = lastRx - firstTx;
        if (duration > 0) {
          throughput = (rxBytes * 8.0) / (duration * 1e6);
        }
        if (rxPackets > 0) {
          delay = (delaySum / rxPackets) * 1000.0;
          jitter = (jitterSum / rxPackets) * 1000.0;
        }
        if (txPackets > rxPackets) {
          drop = txPackets - rxPackets;
        }

        // Simpan untuk agregat
        totalThroughput += throughput;
        totalDelay += delay;
        totalJitter += jitter;
        totalDrop += drop;"""

    # We must be careful about brace balancing. The old block doesn't close the `if (t.destinationAddress...)` brace.
    # The new block closes `if (found)` at the end of the loop iteration.
    # Let's replace until `totalDrop += drop;`
    content = content.replace(old_flow_block, new_flow_block)
    
    # We also need to fix the closing brace for the old `if`.
    # It was closed right before `if (i < 10) {`
    old_brace_block = """      }
    }

    if (i < 10) {"""
    new_brace_block = """    }

    if (i < 10) {"""
    content = content.replace(old_brace_block, new_brace_block)

    with open(filepath, "w") as file:
        file.write(content)

    print(f"Patched {f} successfully.")
