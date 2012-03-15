# smileys: CMakeLists.txt

cmake_minimum_required(VERSION 2.6)

project(smileys C)

add_library(velox_smileys MODULE smileys.c)
set_target_properties(velox_smileys PROPERTIES PREFIX "")
install(TARGETS velox_smileys DESTINATION lib${LIB_SUFFIX}/velox/modules)

