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
    
    # Replace system for outDir
    content = content.replace(
        'std::system(("mkdir -p " + outDir).c_str());',
        'if (std::system(("mkdir -p " + outDir).c_str()) != 0) { std::cerr << "Warning: Failed to create " << outDir << std::endl; }'
    )

    # Replace system for pcapDir
    content = content.replace(
        'std::system(("mkdir -p " + pcapDir).c_str());',
        'if (std::system(("mkdir -p " + pcapDir).c_str()) != 0) { std::cerr << "Warning: Failed to create " << pcapDir << std::endl; }'
    )
    
    with open(filepath, "w") as file:
        file.write(content)
    print(f"Patched system calls in {filepath}")

