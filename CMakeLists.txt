# CMakeLists.txt untuk subdirektori scratch/ta_wifi7
# Mendefinisikan target terpisah untuk setiap file simulasi agar dapat dikompilasi secara independen.

# 1. Target untuk ap_placement
build_exec(
  EXECNAME ap_placement
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES ap_placement.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# 2. Target untuk indoor_wifi7
build_exec(
  EXECNAME indoor_wifi7
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES indoor_wifi7.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

# 3. Target untuk indoor_wifi6
build_exec(
  EXECNAME indoor_wifi6
  EXECNAME_PREFIX scratch_ta_wifi7_
  SOURCE_FILES indoor_wifi6.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/ta_wifi7/
)

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
