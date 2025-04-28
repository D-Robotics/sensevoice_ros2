// Copyright (c) 2024，D-Robotics.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef INCLUDE_HBAUDIOCAPTURE_H_
#define INCLUDE_HBAUDIOCAPTURE_H_

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <alsa/asoundlib.h>
#include "audio_msg/msg/smart_audio_data.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "utils/alsa_device.h"
//#include "speech_engine.h"

namespace hobot {
namespace audio {

using rclcpp::NodeOptions;
class HBAudioCapture : public rclcpp::Node {
 public:
  // node_name为创建的节点名，options为选项，用法和ROS Node相同
  HBAudioCapture(const std::string& node_name,
                 const NodeOptions& options = NodeOptions());

  virtual ~HBAudioCapture();

  int Run();

 public:
  int Init();
  int DeInit();
  int Start();
  int Stop();

 private:
  int MicphoneGetThread();
  void asr_send_th();

  void AudioCmdDataFunc(std::string cmd_word);
  void AudioASRFunc(std::string asr);

 private:
  int micphone_enable_ = 1;
  std::shared_ptr<std::thread> micphone_thread_;
  alsa_device_t* micphone_device_ = nullptr;
  bool exit_ = true;
  bool is_init_ = false;
  int audio_num_ = 0;
  std::string micphone_name_ = "plughw:0,0";
  int micphone_rate_ = 16000;
  int micphone_chn_ = 2;
  int micphone_buffer_time_ = 0;
  int micphone_nperiods_ = 4;
  int micphone_period_size_ = 512;
  int voip_mode_ = 0;
  int mic_type_ = 0;
  int asr_output_mode_ = 0;
  int asr_output_channel_ = 3;
  int push_wakeup_ = 0;

  std::string wakeup_name_ = "你好";
  std::string wakeup_name_1_ = "你好,";
  std::string config_path_ = "./config";
  // audio_sdk_path_ will be updated at runtime with env "TROS_DISTRO"
  std::string asr_model_ = "sense-voice-small-fp16.gguf";
  std::string asr_model_path_ = "";
  std::string audio_pub_topic_name_ = "/audio_smart";
  std::string asr_pub_topic_name_ = "/audio_asr";
  std::ofstream audio_infile_;
  std::ofstream audio_sdk_;
  bool save_audio_ = false;
  std::shared_ptr<std::vector<std::string>> v_cmd_word_;
  std::string cmd_word_path_ = "./config/cmd_word.json";

  rclcpp::Publisher<audio_msg::msg::SmartAudioData>::SharedPtr msg_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr asr_msg_publisher_;
};

}  // namespace audio
}  // namespace hobot

#endif
