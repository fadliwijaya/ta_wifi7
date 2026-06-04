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
    
    # Fix the loop
    content = content.replace("for (uint32_t i = 18; i < 24; ++i)\n    staKoridor", "for (uint32_t i = 22; i < 24; ++i)\n    staKoridor")
    
    with open(filepath, "w") as file:
        file.write(content)
    print(f"Fixed {filepath}")

