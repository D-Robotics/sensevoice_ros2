#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sensevoice_pc {

// ALSA 采集封装（等效原仓库 src/utils/alsa_device.cpp）
class AlsaAudio {
 public:
  AlsaAudio() = default;
  ~AlsaAudio();

  // device: 如 "plughw:0,0"；channels 支持 1 或 2；rate 默认 16000
  bool Open(const std::string& device, int rate, int channels,
            int period_size = 512, int nperiods = 4);
  // 读取一帧，返回 int16 采样（若采集为双声道，保持原始顺序，由上层降混）
  bool Read(std::vector<int16_t>& out, size_t& frames);
  void Close();

  int rate() const { return rate_; }
  int channels() const { return channels_; }
  bool IsOpen() const { return handle_ != nullptr; }

 private:
  void* handle_ = nullptr;  // snd_pcm_t*
  int rate_ = 0;
  int channels_ = 0;
};

}  // namespace sensevoice_pc
