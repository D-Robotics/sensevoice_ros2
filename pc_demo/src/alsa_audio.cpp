#include "alsa_audio.h"

#include <alsa/asoundlib.h>
#include <cstdio>
#include <cstring>

namespace sensevoice_pc {

AlsaAudio::~AlsaAudio() { Close(); }

bool AlsaAudio::Open(const std::string& device, int rate, int channels,
                     int period_size, int nperiods) {
  Close();

  int rc = snd_pcm_open(reinterpret_cast<snd_pcm_t**>(&handle_), device.c_str(),
                        SND_PCM_STREAM_CAPTURE, 0);
  if (rc < 0) {
    std::fprintf(stderr, "[alsa] cannot open device %s: %s\n", device.c_str(),
                 snd_strerror(rc));
    handle_ = nullptr;
    return false;
  }

  snd_pcm_hw_params_t* hw = nullptr;
  snd_pcm_hw_params_alloca(&hw);
  snd_pcm_hw_params_any(reinterpret_cast<snd_pcm_t*>(handle_), hw);

  snd_pcm_hw_params_set_access(reinterpret_cast<snd_pcm_t*>(handle_), hw,
                               SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(reinterpret_cast<snd_pcm_t*>(handle_), hw,
                               SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(reinterpret_cast<snd_pcm_t*>(handle_), hw, channels);
  unsigned int r = rate;
  snd_pcm_hw_params_set_rate_near(reinterpret_cast<snd_pcm_t*>(handle_), hw, &r, 0);
  snd_pcm_hw_params_set_period_size_near(reinterpret_cast<snd_pcm_t*>(handle_), hw,
                                         reinterpret_cast<snd_pcm_uframes_t*>(&period_size), 0);
  snd_pcm_hw_params_set_periods_near(reinterpret_cast<snd_pcm_t*>(handle_), hw,
                                     reinterpret_cast<unsigned int*>(&nperiods), 0);

  rc = snd_pcm_hw_params(reinterpret_cast<snd_pcm_t*>(handle_), hw);
  if (rc < 0) {
    std::fprintf(stderr, "[alsa] hw_params failed: %s\n", snd_strerror(rc));
    Close();
    return false;
  }

  rate_ = static_cast<int>(r);
  channels_ = channels;
  return true;
}

bool AlsaAudio::Read(std::vector<int16_t>& out, size_t& frames) {
  if (!handle_) return false;
  const size_t buf_frames = 512;  // period_size
  out.resize(buf_frames * channels_);
  snd_pcm_t* pcm = reinterpret_cast<snd_pcm_t*>(handle_);
  long n = snd_pcm_readi(pcm, out.data(), buf_frames);
  if (n < 0) {
    n = snd_pcm_recover(pcm, static_cast<int>(n), 1);
    if (n < 0) {
      std::fprintf(stderr, "[alsa] read error: %s\n", snd_strerror(static_cast<int>(n)));
      frames = 0;
      return false;
    }
    n = snd_pcm_readi(pcm, out.data(), buf_frames);
    if (n < 0) {
      frames = 0;
      return false;
    }
  }
  frames = static_cast<size_t>(n);
  out.resize(frames * channels_);
  return true;
}

void AlsaAudio::Close() {
  if (handle_) {
    snd_pcm_close(reinterpret_cast<snd_pcm_t*>(handle_));
    handle_ = nullptr;
  }
}

}  // namespace sensevoice_pc
