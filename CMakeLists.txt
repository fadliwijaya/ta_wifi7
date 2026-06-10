# CMakeLists.txt untuk subdirektori scratch/ta_wifi7
# Mendefinisikan target terpisah untuk setiap file simulasi agar dapat dikompilasi secara independen.


# 4. Target untuk Indoor_wifi7_final_upgrade
build_exec(
  EXECNAME Indoor_wifi7_final_upgrade
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES Indoor_wifi7_final_upgrade.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# 5. Target untuk Indoor_wifi6_baseline
build_exec(
  EXECNAME Indoor_wifi6_baseline
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES Indoor_wifi6_baseline.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# 6. Target untuk Indoor_wifi7_stress_test
build_exec(
  EXECNAME Indoor_wifi7_stress_test
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES Indoor_wifi7_stress_test.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# 7. Target untuk Indoor_wifi6_stress_test
build_exec(
  EXECNAME Indoor_wifi6_stress_test
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES Indoor_wifi6_stress_test.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# 8. Target untuk Indoor_wifi7_pcap
build_exec(
  EXECNAME Indoor_wifi7_pcap
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES Indoor_wifi7_pcap.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# Target untuk Indoor_wifi6_clean
build_exec(
  EXECNAME Indoor_wifi6_clean
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES Indoor_wifi6_clean.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# Target untuk Indoor_wifi7_clean
build_exec(
  EXECNAME Indoor_wifi7_clean
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES Indoor_wifi7_clean.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# Target untuk Indoor_wifi7_roaming_test
build_exec(
  EXECNAME Indoor_wifi7_roaming_test
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES Indoor_wifi7_roaming_test.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# Target untuk Indoor_wifi6_roaming_test
build_exec(
  EXECNAME Indoor_wifi6_roaming_test
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES Indoor_wifi6_roaming_test.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# Target untuk Indoor_wifi7_range_test
build_exec(
  EXECNAME Indoor_wifi7_range_test
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES Indoor_wifi7_range_test.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# Target untuk Indoor_wifi6_range_test
build_exec(
  EXECNAME Indoor_wifi6_range_test
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES Indoor_wifi6_range_test.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# Target untuk Indoor_wifi7_density_test
build_exec(
  EXECNAME Indoor_wifi7_density_test
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES Indoor_wifi7_density_test.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# Target untuk Indoor_wifi6_density_test
build_exec(
  EXECNAME Indoor_wifi6_density_test
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES Indoor_wifi6_density_test.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# Target untuk wifi7_pameran
build_exec(
  EXECNAME wifi7_pameran
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES wifi7_pameran.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# Target untuk wifi6_pameran
build_exec(
  EXECNAME wifi6_pameran
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES wifi6_pameran.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)
