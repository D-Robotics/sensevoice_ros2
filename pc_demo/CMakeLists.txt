cmake_minimum_required(VERSION 3.16)
project(sensevoice_pc_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---- 依赖：ALSA ----
find_package(ALSA REQUIRED)

# ---- 上游 lovemefan/SenseVoice.cpp（开源版，x86 可编译）----
# 优先使用环境变量/命令行指定的路径；否则回退到默认
if(NOT SENSEVOICE_SRC_DIR)
  if(DEFINED ENV{SENSEVOICE_SRC_DIR})
    set(SENSEVOICE_SRC_DIR $ENV{SENSEVOICE_SRC_DIR})
  else()
    set(SENSEVOICE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/SenseVoice.cpp)
  endif()
endif()
message(STATUS "Using SenseVoice.cpp source dir: ${SENSEVOICE_SRC_DIR}")
if(NOT EXISTS ${SENSEVOICE_SRC_DIR}/CMakeLists.txt)
  # 尝试自动 clone
  message(STATUS "Cloning lovemefan/SenseVoice.cpp into ${SENSEVOICE_SRC_DIR} ...")
  execute_process(
    COMMAND git clone --depth 1 --recursive
            https://github.com/lovemefan/SenseVoice.cpp.git
            ${SENSEVOICE_SRC_DIR}
    RESULT_VARIABLE _clone_ret)
  if(NOT _clone_ret EQUAL 0)
    message(FATAL_ERROR
      "Failed to find or clone SenseVoice.cpp.\n"
      "Please clone it manually (with --recurse-submodules) into:\n"
      "  ${SENSEVOICE_SRC_DIR}\n"
      "or pass -DSENSEVOICE_SRC_DIR=/path/to/SenseVoice.cpp")
  endif()
endif()

# 引入上游构建系统（只取库目标 sense-voice-core / ggml，不参与默认 ALL）
# 关闭示例/测试以加快构建
set(SENSE_VOICE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SENSE_VOICE_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
add_subdirectory(${SENSEVOICE_SRC_DIR} ${CMAKE_BINARY_DIR}/upstream EXCLUDE_FROM_ALL)

# ---- 我们的目标 ----
add_executable(sensevoice_pc_demo
  src/main.cpp
  src/alsa_audio.cpp
  src/sense_engine.cpp
  src/tts_engine.cpp
)

target_include_directories(sensevoice_pc_demo PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${SENSEVOICE_SRC_DIR}/sense-voice/csrc
  ${SENSEVOICE_SRC_DIR}/sense-voice/csrc/third-party/ggml/include
  ${SENSEVOICE_SRC_DIR}/sense-voice/csrc/third-party/ggml/src
)

target_link_libraries(sensevoice_pc_demo PRIVATE
  sense-voice-core
  ${ALSA_LIBRARIES}
  pthread
  dl
  m
)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 9.0)
  target_link_libraries(sensevoice_pc_demo PRIVATE stdc++fs)
endif()
