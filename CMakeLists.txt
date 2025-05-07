# Copyright (c) 2024，D-Robotics.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.5)
project(sensevoice_ros2)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

#if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#  add_compile_options(-Wall -Wextra -Wpedantic)
#endif()

# find dependencies
find_package(ament_cmake REQUIRED)
# uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(audio_msg REQUIRED)
find_package(std_msgs REQUIRED)
find_package(jsoncpp REQUIRED)
#find_package(OpenSSL REQUIRED)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/SenseVoiceGGUF")
  execute_process(
      COMMAND git lfs install
      COMMAND git clone https://www.modelscope.cn/lovemefan/SenseVoiceGGUF.git SenseVoiceGGUF
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_QUIET  # 静默输出（避免日志冗余）:ml-citation{ref="2,8" data="citationList"}
      ERROR_VARIABLE git_clone_error  # 捕获错误信息:ml-citation{ref="4,8" data="citationList"}
      RESULT_VARIABLE git_clone_result  # 获取执行结果（0表示成功）:ml-citation{ref="4,8" data="citationList"}
  )
  if(NOT git_clone_result EQUAL 0)
      message(FATAL_ERROR "Git clone failed: ${git_clone_error}")  # 输出详细错误:ml-citation{ref="4,8" data="citationList"}
  endif()
endif()



if (EMSCRIPTEN)
    set(BUILD_SHARED_LIBS_DEFAULT OFF)

    option(GGML_WASM_SINGLE_FILE "ggml: embed WASM inside the generated ggml.js" ON)
else()
    if (MINGW)
        set(BUILD_SHARED_LIBS_DEFAULT OFF)
    else()
        set(BUILD_SHARED_LIBS_DEFAULT ON)
    endif()
endif()

option(BUILD_SHARED_LIBS "build shared libraries" ${BUILD_SHARED_LIBS_DEFAULT})

# general
option(SENSE_VOICE_CCACHE "sense-voice: use ccache if available" ON)

# debug
option(SENSE_VOICE_ALL_WARNINGS           "sense-voice: enable all compiler warnings"                   ON)
option(SENSE_VOICE_ALL_WARNINGS_3RD_PARTY "sense-voice: enable all compiler warnings in 3rd party libs" OFF)

option(SENSE_VOICE_BUILD_TESTS "sense-voice: build tests" OFF)

# sanitizers
option(SENSE_VOICE_SANITIZE_THREAD    "sense-voice: enable thread sanitizer"    OFF)
option(SENSE_VOICE_SANITIZE_ADDRESS   "sense-voice: enable address sanitizer"   OFF)
option(SENSE_VOICE_SANITIZE_UNDEFINED "sense-voice: enable undefined sanitizer" OFF)

# extra artifacts
option(SENSE_VOICE_BUILD_EXAMPLES "whisper: build examples"       ON)

# build
option(SENSE_VOICE_FATAL_WARNINGS "sense-voice: enable -Werror flag" OFF)

option(GGML_USE_BLAS                     "usr blas"  OFF)
option(SENSE_VOICE_COREML                "sense voice: enable Core ML framework"  OFF)
option(SENSE_VOICE_COREML_ALLOW_FALLBACK "sense voice: allow non-CoreML fallback" OFF)
option(SENSE_VOICE_OPENVINO              "sense voice: support for OpenVINO"      OFF)

# override ggml options
set(GGML_CCACHE             ${SENSE_VOICE_CCACHE})
set(GGML_SANITIZE_THREAD    ${SENSE_VOICE_SANITIZE_THREAD})
set(GGML_SANITIZE_ADDRESS   ${SENSE_VOICE_SANITIZE_ADDRESS})
set(GGML_SANITIZE_UNDEFINED ${SENSE_VOICE_SANITIZE_UNDEFINED})
set(GGML_ALL_WARNINGS       ${SENSE_VOICE_ALL_WARNINGS})
set(GGML_FATAL_WARNINGS     ${SENSE_VOICE_FATAL_WARNINGS})

# Required for relocatable CMake package
#include(${CMAKE_CURRENT_SOURCE_DIR}/include/sensevoice/lib/cmake/ggml/ggml-version.cmake)
#include(${CMAKE_CURRENT_SOURCE_DIR}/include/sensevoice/lib/cmake/ggml/ggml-config.cmake)


if(GGML_USE_BLAS)
    add_definitions(-DGGML_USE_BLAS)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
set(CMAKE_BUILD_TYPE "Release")
#find_package(ggml REQUIRED)


message("PREFIX_PATH: ${PREFIX_PATH}")

include_directories(include
${PROJECT_SOURCE_DIR}
)

set(SYS_ROOT ${CMAKE_SYSROOT})
message("SYS_ROOT is " ${SYS_ROOT})
include_directories(
  ${SYS_ROOT}/usr/include/
  ${SYS_ROOT}/usr/include/jsoncpp/
  ${PROJECT_SOURCE_DIR}/include/sensevoice/include
)

link_directories(
  ${PROJECT_SOURCE_DIR}/include/sensevoice/lib
  ${SYS_ROOT}/usr/lib/
  ${SYS_ROOT}/usr/lib/hobot/
  ${SYS_ROOT}/usr/hobot/lib/
  ${SYS_ROOT}/usr/lib/hobot/hobotlog
)

add_executable(${PROJECT_NAME}
  src/utils/alsa_device.cpp
  src/hb_audio_capture.cpp
  src/speech_engine.cpp
  src/audio_example.cpp
)

#set(BASE_LIBRARIES pthread dl rt m)
set(BASE_LIBRARIES pthread)
set(X3_AUDIO_LIB asound sense-voice-core ggml-cpu ggml-base ggml jsoncpp)

target_link_libraries(${PROJECT_NAME}
   ${X3_AUDIO_LIB} ${X3_HAPI_LIB} ${BASE_LIBRARIES}
)

ament_target_dependencies(${PROJECT_NAME}
"rclcpp"
"geometry_msgs"
"audio_msg")

ament_target_dependencies(${PROJECT_NAME} audio_msg)

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

# Install libraries
install(DIRECTORY include/sensevoice/lib
DESTINATION ./)

# Install libraries
install(TARGETS ${PROJECT_NAME}
  DESTINATION lib/${PROJECT_NAME}/)

# Install include directories
install(
  DIRECTORY config
  DESTINATION lib/${PROJECT_NAME}/
)

# Install model directories
install(
  FILES SenseVoiceGGUF/sense-voice-small-fp16.gguf
  DESTINATION lib/${PROJECT_NAME}/model/
)

# Install model directories
install(
  FILES SenseVoiceGGUF/sense-voice-small-fp32.gguf
  DESTINATION lib/${PROJECT_NAME}/model/
)

# Install model directories
install(
  FILES SenseVoiceGGUF/sense-voice-small-q4_k.gguf
  DESTINATION lib/hobot_asr/model/
)

install(DIRECTORY
${PROJECT_SOURCE_DIR}/launch/
DESTINATION share/${PROJECT_NAME}/launch)

# Causes the visibility macros to use dllexport rather than dllimport,
# which is appropriate when building the dll but not consuming it.
target_compile_definitions(${PROJECT_NAME}
  PRIVATE "RCLCPP_BUILDING_LIBRARY")

install(
  TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# specific order: dependents before dependencies
ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME})
ament_export_targets(${PROJECT_NAME})

ament_package()
