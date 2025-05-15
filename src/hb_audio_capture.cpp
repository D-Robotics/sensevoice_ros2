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

#include "hb_audio_capture.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include "speech_engine.h"
#include <json/json.h>

namespace hobot {
namespace audio {
HBAudioCapture::HBAudioCapture(const std::string &node_name,
                               const NodeOptions &options)
    : rclcpp::Node(node_name, options) {
  
  std::string tros_distro
      = std::string(std::getenv("TROS_DISTRO")? std::getenv("TROS_DISTRO") : "");
  asr_model_path_ = "/opt/tros/" + tros_distro + "/lib/sensevoice_ros2/model/";
  //asr_model_path_ = "./install/lib/sensevoice_ros2/model/";
  cmd_word_path_ = "/opt/tros/" + tros_distro + "/lib/sensevoice_ros2/config/cmd_word.json";
  //cmd_word_path_ = "./install/lib/sensevoice_ros2/config/cmd_word.json";

  this->declare_parameter<std::string>("micphone_name",
                                       micphone_name_);
  this->declare_parameter<std::string>("audio_pub_topic_name",
                                       audio_pub_topic_name_);
  this->declare_parameter<std::string>("asr_pub_topic_name",
                                       asr_pub_topic_name_);
  this->declare_parameter<std::string>("asr_model",
                                       asr_model_);
  this->declare_parameter<int>("push_wakeup",
                                       push_wakeup_);
  this->declare_parameter<std::string>("wakeup_name",
                                       wakeup_name_);

  this->get_parameter<std::string>("micphone_name",
                                   micphone_name_);
  this->get_parameter<std::string>("audio_pub_topic_name",
                                   audio_pub_topic_name_);
  this->get_parameter<std::string>("asr_pub_topic_name",
                                   asr_pub_topic_name_);
  this->get_parameter<std::string>("asr_model",
                                   asr_model_);
  this->get_parameter<int>("push_wakeup",
                                   push_wakeup_);
  this->get_parameter<std::string>("wakeup_name",
                                   wakeup_name_);
  
  if (wakeup_name_.length() > 0) {
    wakeup_name_1_ = wakeup_name_ + ",";
  }

  asr_model_path_ += asr_model_;
  std::stringstream ss;
  ss << "Parameter:"
     << "\n micphone_name: " << micphone_name_
     << "\n audio_pub_topic_name: " << audio_pub_topic_name_
     << "\n asr_pub_topic_name: " << asr_pub_topic_name_
     << "\n asr_model_path_: " << asr_model_path_
     << "\n push_wakeup: " << push_wakeup_;
  RCLCPP_WARN(rclcpp::get_logger("sensevoice_ros2"), "%s", ss.str().c_str());
}

HBAudioCapture::~HBAudioCapture() { DeInit(); }

int HBAudioCapture::Init() {
  v_cmd_word_ = std::make_shared<std::vector<std::string>>();
  std::ifstream cmd_word(cmd_word_path_);
  if (cmd_word.is_open()) {
    Json::Value root;
    cmd_word >> root;
    if (root.isMember("cmd_word") && root["cmd_word"].isArray()) {
      const Json::Value& cmdWords = root["cmd_word"];
      for (const auto& word : cmdWords) {
        std::cout << "命令词: " << word.asString() << std::endl;
        v_cmd_word_->push_back(word.asString());
      }
    }
    cmd_word.close();
  }  

  RCLCPP_INFO(rclcpp::get_logger("sensevoice_ros2"), "init to capture audio");
  micphone_device_ = alsa_device_allocate();
  if (!micphone_device_) {
    RCLCPP_INFO(rclcpp::get_logger("sensevoice_ros2"), "open mic device fail");
    return -1;
  }
  /* init micphone device*/
  micphone_device_->name = const_cast<char *>(micphone_name_.c_str());
  micphone_device_->format = SND_PCM_FORMAT_S16;
  micphone_device_->direct = SND_PCM_STREAM_CAPTURE;
  micphone_device_->rate = micphone_rate_;
  micphone_device_->channels = micphone_chn_;
  micphone_device_->buffer_time = micphone_buffer_time_;
  micphone_device_->nperiods = micphone_nperiods_;
  micphone_device_->period_size = micphone_period_size_;
  int ret = alsa_device_init(micphone_device_);
  if (ret < 0) {
    RCLCPP_ERROR(rclcpp::get_logger("sensevoice_ros2"),
                 "alsa device init fail, ret=%d", ret);
    return -1;
  }

  RCLCPP_WARN_STREAM(rclcpp::get_logger("sensevoice_ros2"),
    "asr_model_path_ is [" << asr_model_path_ << "]");
   speech_engine::Instance()->Init(asr_model_path_, wakeup_name_, v_cmd_word_,
       std::bind(&HBAudioCapture::AudioASRFunc, this, std::placeholders::_1),
       std::bind(&HBAudioCapture::AudioCmdDataFunc, this, std::placeholders::_1));

  RCLCPP_WARN(rclcpp::get_logger("sensevoice_ros2"), "init success");
  // system("rm ./*.pcm -rf");
  if (save_audio_) {
    audio_infile_.open("./audio_in.pcm",
                       std::ios::app | std::ios::out | std::ios::binary);
  }
  msg_publisher_ = this->create_publisher<audio_msg::msg::SmartAudioData>(
      audio_pub_topic_name_, 10);
  asr_msg_publisher_ = this->create_publisher<std_msgs::msg::String>(asr_pub_topic_name_, 10);
  is_init_ = true;
  return 0;
}

int HBAudioCapture::DeInit() {
  RCLCPP_INFO(rclcpp::get_logger("sensevoice_ros2"), "deinit");
  if (!is_init_) return 0;
  if (!micphone_device_) return -1;
  if (micphone_device_) {
    alsa_device_deinit(micphone_device_);
    alsa_device_free(micphone_device_);
    micphone_device_ = nullptr;
  }
  speech_engine::Instance()->Stop();
  speech_engine::Instance()->DeInit();
  if (audio_infile_.is_open()) {
    audio_infile_.close();
  }
  if (audio_sdk_.is_open()) {
    audio_sdk_.close();
  }
  return 0;
}

int HBAudioCapture::Run() {
  if (!is_init_) {
    RCLCPP_ERROR(rclcpp::get_logger("sensevoice_ros2"), "HBAudioCapture not init.");
    return -1;
  }

  speech_engine::Instance()->Start();
  rclcpp::executors::SingleThreadedExecutor exec;
  auto capture_task = std::make_shared<std::thread>(
      std::bind(&HBAudioCapture::MicphoneGetThread, this));
  exec.spin();
  if (capture_task && capture_task->joinable()) {
    capture_task.reset();
  }
  exec.spin();
  return 0;
}

int HBAudioCapture::MicphoneGetThread() {
  RCLCPP_WARN(rclcpp::get_logger("sensevoice_ros2"), "start to capture audio");
  if (!micphone_device_) {
    RCLCPP_ERROR(rclcpp::get_logger("sensevoice_ros2"), "micphone device is null");
    return -1;
  }

  int ret = -1;
  snd_pcm_sframes_t frames;
  frames = micphone_device_->period_size;
  int buffer_size = snd_pcm_frames_to_bytes(micphone_device_->handle, frames);

  std::cout << "MicphoneGetThread------buffer_size:" << buffer_size << std::endl;
  char *buffer = new char[buffer_size];
  while (rclcpp::ok()) {
    // auto start_time = std::chrono::high_resolution_clock::now();
    ret = alsa_device_read(micphone_device_, buffer, frames);
    // auto end_time = std::chrono::high_resolution_clock::now();
    // auto cost_time = std::chrono::duration_cast<std::chrono::microseconds>(
    //     end_time - start_time).count();
    if (ret <= 0) continue;
    RCLCPP_DEBUG(rclcpp::get_logger("sensevoice_ros2"), "capture audio buffer_size:%d",
                 buffer_size);
    audio_num_++;

    int data_audio_size = buffer_size / 2 / 2;
    auto vec_ptr = std::make_shared<std::vector<double>>();
    int16_t *src_ptr = (int16_t *)buffer;
    for (int i = 0; i < data_audio_size; i++) {
      vec_ptr->push_back((double)(src_ptr[i * 2 + 1]));
    }
    speech_engine::Instance()->send_data(vec_ptr);
    if (save_audio_ && audio_infile_.is_open()) {
      audio_infile_.write(buffer, buffer_size);
    }
  }
  RCLCPP_WARN(rclcpp::get_logger("sensevoice_ros2"), "stop capture audio");
  delete[] buffer;
  return 0;
}

void HBAudioCapture::AudioCmdDataFunc(std::string cmd_word) {
  RCLCPP_WARN(rclcpp::get_logger("sensevoice_ros2"), "recv cmd word:%s", cmd_word.c_str());
  audio_msg::msg::SmartAudioData::UniquePtr frame(new audio_msg::msg::SmartAudioData());
  frame->frame_type.value = frame->frame_type.SMART_AUDIO_TYPE_CMD_WORD;
  frame->cmd_word = cmd_word;
  msg_publisher_->publish(std::move(frame));
}

void HBAudioCapture::AudioASRFunc(std::string asr) {
  if (asr.length() > 0) {
    RCLCPP_WARN(rclcpp::get_logger("sensevoice_ros2"), "asr msg:%s", asr.c_str());
    if ((push_wakeup_) && (asr == wakeup_name_)) {
      auto message = std::make_unique<std_msgs::msg::String>();
      message->data = asr;
      RCLCPP_WARN(rclcpp::get_logger("sensevoice_ros2"), "asr publish:%s", asr.c_str());
      asr_msg_publisher_->publish(std::move(message));
    }
    size_t pos = asr.find(wakeup_name_, 0);  
    size_t pos1 = asr.find(wakeup_name_1_, 0);      
    if (pos1 != std::string::npos) {
      if (pos1 < (asr.length() - wakeup_name_1_.length())) {
        std::string asr_msg;
        asr_msg.append(asr, pos1 + wakeup_name_1_.length(), asr.length() - pos1 - wakeup_name_1_.length());
        auto message = std::make_unique<std_msgs::msg::String>();
        message->data = asr_msg;
        RCLCPP_WARN(rclcpp::get_logger("sensevoice_ros2"), "asr publish:%s", asr_msg.c_str());
        asr_msg_publisher_->publish(std::move(message));
      }
    } else if (pos != std::string::npos) {
      if (pos < (asr.length() - wakeup_name_.length())) {
        std::string asr_msg;
        asr_msg.append(asr, pos + wakeup_name_.length(), asr.length() - pos - wakeup_name_.length());
        auto message = std::make_unique<std_msgs::msg::String>();
        message->data = asr_msg;
        RCLCPP_WARN(rclcpp::get_logger("sensevoice_ros2"), "asr publish:%s", asr_msg.c_str());
        asr_msg_publisher_->publish(std::move(message));
      }
    }
  }
}

}  // namespace audio
}  // namespace hobot
