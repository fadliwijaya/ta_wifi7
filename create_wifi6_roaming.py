import re

with open("scratch/ta_wifi7/Indoor_wifi7_roaming_test.cc", "r") as f:
    content = f.read()

# 1. Standard to 802.11ax
content = content.replace("WIFI_STANDARD_80211be", "WIFI_STANDARD_80211ax")

# 2. Text strings
content = content.replace("Wi-Fi 7 Indoor MLO", "Wi-Fi 6 Indoor")
content = content.replace("wifi7", "wifi6")
content = content.replace("WIFI 7", "WIFI 6")
content = content.replace("Wi-Fi 7", "Wi-Fi 6")

# 3. Remove 6GHz Channel and MLO
# Remove 6GHz Spectrum Setup
content = re.sub(r'Ptr<MultiModelSpectrumChannel> channel6Ghz.*?spectrumPhy\.AddChannel\(channel6Ghz, WIFI_SPECTRUM_6_GHZ\);\n', '', content, flags=re.DOTALL)

# Change spectrumPhy 5GHz from index 0 to no index
content = content.replace('spectrumPhy.Set(0, "ChannelSettings"', 'spectrumPhy.Set("ChannelSettings"')
content = content.replace('spectrumPhy.AddChannel(channel5Ghz, WIFI_SPECTRUM_5_GHZ);', 'spectrumPhy.SetChannel(channel5Ghz);')

# Remove MLO MAC settings
content = re.sub(r'mac\.SetMultiLinkType\(ns3::WifiMacHelper::WIFI_MAC_MLD\);\n', '', content)
content = re.sub(r'apMac\.SetMultiLinkType\(ns3::WifiMacHelper::WIFI_MAC_MLD\);\n', '', content)

# 4. Fix Mac addresses logic for StaAssocCallback
# In Wi-Fi 7: Ptr<StaWifiMac> staMac = DynamicCast<StaWifiMac>(dev->GetMac()); ... bssid = staMac->GetLinkMacAddress(0);
# In Wi-Fi 6: bssid = staMac->GetBssid();
# But wait, actually GetBssid() is correct for both if it's a single link, but let's carefully replace it.
content = content.replace('Mac48Address bssid = staMac->GetLinkBssid(0);', 'Mac48Address bssid = staMac->GetBssid();')
content = content.replace('Mac48Address bssid = staMac->GetLinkBssid(1);', 'Mac48Address bssid = staMac->GetBssid();')

# Fix any leftover MLD / Link references in callback
# If there's `staMac->GetLinkMacAddress(0)`
content = content.replace('staMac->GetLinkMacAddress(0)', 'staMac->GetBssid()')

with open("scratch/ta_wifi7/Indoor_wifi6_roaming_test.cc", "w") as f:
    f.write(content)

print("Created Indoor_wifi6_roaming_test.cc")
