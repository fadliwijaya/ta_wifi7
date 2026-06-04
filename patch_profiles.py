import os
import re

files = [
    "Indoor_wifi6_baseline.cc",
    "Indoor_wifi6_stress_test.cc",
    "Indoor_wifi7_final_upgrade.cc",
    "Indoor_wifi7_stress_test.cc",
    "Indoor_wifi7_pcap.cc"
]

def patch_file(filepath):
    with open(filepath, "r") as file:
        content = file.read()

    # 1. Update struct UserProfile
    struct_pattern = r'struct UserProfile \{\s*std::string name;\s*uint16_t port;\s*uint32_t packetSize;\s*// Bytes\s*std::string dataRate;\s*uint32_t tos;\s*// IP Type of Service.*?\s*std::string acName;.*?\}'
    
    # Simple replace if regex fails
    if 'std::string ulDataRate;' not in content:
        content = re.sub(r'std::string dataRate;\n\s*uint32_t tos;', 'std::string dataRate;\n  std::string ulDataRate;\n  uint32_t tos;', content)

    # 2. Update profiles array
    old_profiles = r'std::vector<UserProfile> profiles = \{\s*\{"Social Media", 9001, 200, "25Mbps", 0x00, "AC_BE"\},.*?\{"Web Browsing", 9005, 1000, "25Mbps", 0x00, "AC_BE"\}\};'
    new_profiles = """std::vector<UserProfile> profiles = {
      {"Social Media", 9001, 200, "25Mbps", "5Mbps", 0x00, "AC_BE"},
      {"Video 4K Streaming", 9002, 1472, "100Mbps", "5Mbps", 0xa0, "AC_VI"},
      {"Gaming", 9003, 1472, "50Mbps", "20Mbps", 0xc0, "AC_VO"},
      {"File Download", 9004, 1472, "150Mbps", "10Mbps", 0x20, "AC_BK"},
      {"Web Browsing", 9005, 1000, "25Mbps", "5Mbps", 0x00, "AC_BE"},
      {"Live Streaming", 9006, 1472, "10Mbps", "50Mbps", 0xa0, "AC_VI"}};"""
    
    content = re.sub(old_profiles, new_profiles, content, flags=re.DOTALL)

    # 3. Update STA counts
    content = content.replace("wifiStaNode.Create(20)", "wifiStaNode.Create(24)")
    content = content.replace("Total 20 STA (10 Kelas A, 8 Kelas B, 2 Koridor)", "Total 24 STA (12 Kelas A, 10 Kelas B, 2 Koridor)")
    content = content.replace("20 User (10 Kelas A, 8 Kelas B, 2 Koridor)", "24 User (12 Kelas A, 10 Kelas B, 2 Koridor)")
    content = content.replace("i < 20;", "i < 24;")
    content = content.replace("i < 10;", "i < 12;")
    content = content.replace("i < 10)", "i < 12)")
    content = content.replace("i = 10; i < 18", "i = 12; i < 22")
    content = content.replace("i = 10; i < 20", "i = 12; i < 24")
    content = content.replace("i = 18; i < 20", "i = 22; i < 24")
    content = content.replace("i < 18)", "i < 22)")
    
    # 4. Modulo operations (i % 5 to i % 6)
    content = content.replace("i % 5", "i % 6")

    # 5. Fix strings in cout/comments
    content = content.replace("STA 0-9", "STA 0-11")
    content = content.replace("STA 10-17", "STA 12-21")
    content = content.replace("STA 18-19", "STA 22-23")

    # 6. Update Uplink DataRate assignment
    old_ul_rate = r'std::string ulRateStr = std::to_string\(baseRate\.GetBitRate\(\) / 2\) \+ "bps"; // 50% of base rate\n\s*onoffUl\.SetAttribute\("DataRate", StringValue\(ulRateStr\)\);'
    new_ul_rate = 'onoffUl.SetAttribute("DataRate", StringValue(profiles[profileIdx].ulDataRate));'
    content = re.sub(old_ul_rate, new_ul_rate, content)

    # Ensure we didn't miss alternate old ul_rate logic
    old_ul_rate_alt = r'std::string ulRateStr = std::to_string\(baseRate\.GetBitRate\(\) / 4\) \+ "bps";.*?\n\s*onoffUl\.SetAttribute\("DataRate", StringValue\(ulRateStr\)\);'
    content = re.sub(old_ul_rate_alt, new_ul_rate, content)

    # 7. Update area strings
    content = content.replace('area = (i < 12) ? "Kelas A" : (i < 22) ? "Kelas B" : "Koridor";', 'area = (i < 12) ? "Kelas A" : (i < 22) ? "Kelas B" : "Koridor";')

    with open(filepath, "w") as file:
        file.write(content)
    print(f"Patched {filepath}")

for f in files:
    patch_file(os.path.join("scratch/ta_wifi7", f))

