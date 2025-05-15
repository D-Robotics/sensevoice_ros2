
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <stdlib.h>

#include "speech_engine.h"

#define CHUNK_SIZE 512
#define CONTEXT_SIZE 576
#define SENSE_VOICE_VAD_CHUNK_PAD_SIZE 64
#define VAD_LSTM_STATE_MEMORY_SIZE 2048
#define VAD_LSTM_STATE_DIM 128

int wkp_count = 0;


/**
 * This the arbitrary data which will be passed to each callback.
 * Later on we can for example add operation or tensor name filter from the CLI arg, or a file descriptor to dump the tensor.
 */
struct callback_data {
    std::vector<uint8_t> data;
};

static std::string ggml_ne_string(const ggml_tensor * t) {
    std::string str;
    for (int i = 0; i < GGML_MAX_DIMS; ++i) {
        str += std::to_string(t->ne[i]);
        if (i + 1 < GGML_MAX_DIMS) {
            str += ", ";
        }
    }
    return str;
}

static void ggml_print_tensor(uint8_t * data, ggml_type type, const int64_t * ne, const size_t * nb, int64_t n) {
    GGML_ASSERT(n > 0);
    float sum = 0;
    for (int64_t i3 = 0; i3 < ne[3]; i3++) {
        printf("                                     [\n");
        for (int64_t i2 = 0; i2 < ne[2]; i2++) {
            if (i2 == n && ne[2] > 2*n) {
                printf("                                      ..., \n");
                i2 = ne[2] - n;
            }
            printf("                                      [\n");
            for (int64_t i1 = 0; i1 < ne[1]; i1++) {
                if (i1 == n && ne[1] > 2*n) {
                    printf("                                       ..., \n");
                    i1 = ne[1] - n;
                }
                printf("                                       [");
                for (int64_t i0 = 0; i0 < ne[0]; i0++) {
                    if (i0 == n && ne[0] > 2*n) {
                        printf("..., ");
                        i0 = ne[0] - n;
                    }
                    size_t i = i3 * nb[3] + i2 * nb[2] + i1 * nb[1] + i0 * nb[0];
                    float v = 0;
                    if (type == GGML_TYPE_F16) {
                        v = ggml_fp16_to_fp32(*(ggml_fp16_t *) &data[i]);
                    } else if (type == GGML_TYPE_F32) {
                        v = *(float *) &data[i];
                    } else if (type == GGML_TYPE_I32) {
                        v = (float) *(int32_t *) &data[i];
                    } else if (type == GGML_TYPE_I16) {
                        v = (float) *(int16_t *) &data[i];
                    } else if (type == GGML_TYPE_I8) {
                        v = (float) *(int8_t *) &data[i];
                    } else {
                        printf("fatal error");
                    }
                    printf("%12.4f", v);
                    sum += v;
                    if (i0 < ne[0] - 1) printf(", ");
                }
                printf("],\n");
            }
            printf("                                      ],\n");
        }
        printf("                                     ]\n");
        printf("                                     sum = %f\n", sum);
    }
    return;
}

/**
 * GGML operations callback during the graph execution.
 *
 * @param t current tensor
 * @param ask when ask is true, the scheduler wants to know if we are interested in data from this tensor
 *            if we return true, a follow-up call will be made with ask=false in which we can do the actual collection.
 *            see ggml_backend_sched_eval_callback
 * @param user_data user data to pass at each call back
 * @return true to receive data or continue the graph, false otherwise
 */
static bool ggml_debug(struct ggml_tensor * t, bool ask, void * user_data) {
    auto * cb_data = (callback_data *) user_data;

    const struct ggml_tensor * src0 = t->src[0];
    const struct ggml_tensor * src1 = t->src[1];

    if (ask) {
        return true; // Always retrieve data
    }

    char src1_str[128] = {0};
    if (src1) {
        snprintf(src1_str, sizeof(src1_str), "%s{%s}", src1->name, ggml_ne_string(src1).c_str());
    }

    printf("%s: %24s = (%s) %10s(%s{%s}, %s}) = {%s}\n", __func__,
           t->name, ggml_type_name(t->type), ggml_op_desc(t),
           src0->name, ggml_ne_string(src0).c_str(),
           src1 ? src1_str : "",
           ggml_ne_string(t).c_str());


    // copy the data from the GPU memory if needed
    const bool is_host = ggml_backend_buffer_is_host(t->buffer);

    if (!is_host) {
        auto n_bytes = ggml_nbytes(t);
        cb_data->data.resize(n_bytes);
        ggml_backend_tensor_get(t, cb_data->data.data(), 0, n_bytes);
    }

    if (!ggml_is_quantized(t->type)) {
        uint8_t * data = is_host ? (uint8_t *) t->data : cb_data->data.data();
        ggml_print_tensor(data, t->type, t->ne, t->nb, 3);
    }

    return true;
}

void sense_voice_free(struct sense_voice_context * ctx) {
    if (ctx) {
        ggml_free(ctx->model.ctx);
        //ggml_free(ctx->vad_model.ctx); // not used.
        ggml_backend_buffer_free(ctx->model.buffer);
        ggml_backend_buffer_free(ctx->vad_model.buffer);

        sense_voice_free_state(ctx->state);

        delete ctx->model.model->encoder;
        delete ctx->model.model;
        delete ctx->vad_model.model;
        delete ctx;
    }
    return;
}


int speech_engine::Init(const std::string &cfg_path, const std::string &wakeup_name,
                        std::shared_ptr<std::vector<std::string>> v_cmd_word,
                        AudioASRFunc asr_func, AudioCmdDataFunc cmd_func) {
  v_cmd_word_ = v_cmd_word;
  audio_asr_cb_ = asr_func;
  audio_cmd_cb_ = cmd_func;
  params.model = cfg_path;
  wakeup_name_ = wakeup_name;
  vad_pad.resize(64, 0.0f);
  vad_mute.clear();
  struct sense_voice_context_params cparams;
  memset(&cparams, 0, sizeof(struct sense_voice_context_params));
  cparams = sense_voice_context_default_params();

  callback_data cb_data;

  memset(&cb_data, 0, sizeof(callback_data));

  cparams.cb_eval = ggml_debug;
  cparams.cb_eval_user_data = &cb_data;

  cparams.use_gpu    = params.use_gpu;
  cparams.flash_attn = params.flash_attn;
  cparams.use_itn    = params.use_itn;
  
  ctx = sense_voice_small_init_from_file_with_params(params.model.c_str(), cparams);
  if (ctx == nullptr) {
      fprintf(stderr, "error: failed to initialize sense voice context\n");
      return -1;
  }

  ctx->language_id = sense_voice_lang_id(params.language.c_str());
  wparams = sense_voice_full_default_params(SENSE_VOICE_SAMPLING_GREEDY);
  wparams.strategy = (params.beam_size > 1 ) ? SENSE_VOICE_SAMPLING_BEAM_SEARCH : SENSE_VOICE_SAMPLING_GREEDY;
  wparams.print_progress   = params.print_progress;
  wparams.print_timestamps = !params.no_timestamps;
  wparams.language         = params.language.c_str();
  wparams.n_threads        = params.n_threads;
  wparams.n_max_text_ctx   = params.max_context >= 0 ? params.max_context : wparams.n_max_text_ctx;
  wparams.offset_ms        = params.offset_t_ms;
  wparams.duration_ms      = params.duration_ms;
  wparams.debug_mode       = params.debug_mode;
  wparams.greedy.best_of        = params.best_of;
  wparams.beam_search.beam_size = params.beam_size;
  wparams.no_timestamps    = params.no_timestamps;


  // init state
  ctx->state->vad_ctx = ggml_init({VAD_LSTM_STATE_MEMORY_SIZE, nullptr, true});
  ctx->state->vad_lstm_context = ggml_new_tensor_1d(ctx->state->vad_ctx, GGML_TYPE_F32, VAD_LSTM_STATE_DIM);
  ctx->state->vad_lstm_hidden_state = ggml_new_tensor_1d(ctx->state->vad_ctx, GGML_TYPE_F32, VAD_LSTM_STATE_DIM);
  ctx->state->vad_lstm_context_buffer = ggml_backend_alloc_buffer(ctx->state->backends[0],
                                                                  ggml_nbytes(ctx->state->vad_lstm_context)
                                                                          + ggml_backend_get_alignment(ctx->state->backends[0]));
  ctx->state->vad_lstm_hidden_state_buffer = ggml_backend_alloc_buffer(ctx->state->backends[0],
                                                                        ggml_nbytes(ctx->state->vad_lstm_hidden_state)
                                                                                + ggml_backend_get_alignment(ctx->state->backends[0]));
  auto context_alloc = ggml_tallocr_new(ctx->state->vad_lstm_context_buffer);
  ggml_tallocr_alloc(&context_alloc, ctx->state->vad_lstm_context);
  auto state_alloc = ggml_tallocr_new(ctx->state->vad_lstm_hidden_state_buffer);
  ggml_tallocr_alloc(&state_alloc, ctx->state->vad_lstm_hidden_state);

  ggml_set_zero(ctx->state->vad_lstm_context);
  ggml_set_zero(ctx->state->vad_lstm_hidden_state);

  fprintf(stdout, "hrsc init success ! \n");
  return 0;
}


int speech_engine::DeInit() {
  if (ctx) {
    sense_voice_free(ctx);
    ctx = nullptr;
  }
  return 0;
}

int speech_engine::Start() {
  fprintf(stdout, "hrsc start success ! \n");
  stop_flag = 0;
  process_thread = std::thread(&speech_engine::process, this);
  return 0;
}

int speech_engine::Stop() {
  fprintf(stdout, "befor stop sdk ! \n");
  stop_flag = true;
  cv.notify_all();
  process_thread.join();
  return 0;
}

void speech_engine::send_data(std::shared_ptr<std::vector<double>> data) {
  if (data->size() == 0) return;
  int size = data->size();
  std::vector<float> chunk;
  chunk.resize(2 * SENSE_VOICE_VAD_CHUNK_PAD_SIZE + size);
  memcpy((char *)chunk.data(), (char *)vad_pad.data(), SENSE_VOICE_VAD_CHUNK_PAD_SIZE * sizeof(float));
  for (int i = 0; i < size; i++) {
    chunk[i + SENSE_VOICE_VAD_CHUNK_PAD_SIZE] = (float)(data->at(i) / 32768);
  }
  int context_size = SENSE_VOICE_VAD_CHUNK_PAD_SIZE + size;
  for (int j = context_size; j < chunk.size(); j++) {
    chunk[j] = chunk[2 * context_size - j - 2];
  }
  memcpy((char *)vad_pad.data(), (char *)(chunk.data() + (context_size - SENSE_VOICE_VAD_CHUNK_PAD_SIZE)), SENSE_VOICE_VAD_CHUNK_PAD_SIZE * sizeof(float));

  float speech_prob = 0;
  silero_vad_encode_internal(*ctx, *ctx->state, chunk, params.n_threads, speech_prob);
  if (triggered) {
    if (speech_prob < params.neg_threshold) {
      vad_stop = std::chrono::system_clock::now();
      auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(vad_stop - vad_pre).count();
      auto interval1 = std::chrono::duration_cast<std::chrono::milliseconds>(vad_stop - vad_send).count();
      auto interval2 = std::chrono::duration_cast<std::chrono::milliseconds>(vad_stop - vad_start).count();
      if (interval >= 98) {
        //std::cout << "send_data--------disable" << std::endl;
        if ((interval2 > params.min_speech_duration_ms) && (vad_data_ptr)) {
          std::lock_guard<std::mutex> lock(mutex);
          if (process_queue.size() >= kMaxQueueSize) {
            process_queue.pop();
          }
          process_queue.push(std::make_pair(vad_data_ptr, 0));
          cv.notify_one();
        }
        vad_data_ptr = nullptr;
        vad_mute.clear();
        triggered = 0;
        asr_final = true;
      } else {
        vad_mute.insert(vad_mute.end(),  std::make_move_iterator(data->begin()), std::make_move_iterator(data->end()));
      }
    } else {
      if (vad_data_ptr == nullptr) {
        vad_data_ptr = std::make_shared<std::vector<double>>();
      }
      if (vad_mute.size() > 0) {
        vad_data_ptr->insert(vad_data_ptr->end(),  std::make_move_iterator(vad_mute.begin()), std::make_move_iterator(vad_mute.end()));
        vad_mute.clear();
      }
      vad_data_ptr->insert(vad_data_ptr->end(),  std::make_move_iterator(data->begin()), std::make_move_iterator(data->end()));
      vad_pre = std::chrono::system_clock::now();
      auto interval2 = std::chrono::duration_cast<std::chrono::milliseconds>(vad_pre - vad_send).count();
      if (interval2 >= params.max_speech_duration_ms) {
        std::lock_guard<std::mutex> lock(mutex);
        if (process_queue.size() >= kMaxQueueSize) {
          process_queue.pop();
        }
        process_queue.push(std::make_pair(vad_data_ptr, 0));
        cv.notify_one();
        vad_data_ptr = nullptr;
        vad_send = vad_pre;
      }
    }

  } else if (speech_prob >= params.threshold) {
    vad_pre = vad_send = vad_start = std::chrono::system_clock::now();
    triggered = 1;
    if (vad_data_ptr == nullptr) {
      vad_data_ptr = std::make_shared<std::vector<double>>();
    }
    if (vad_mute.size() > 0) {
      vad_data_ptr->insert(vad_data_ptr->end(),  std::make_move_iterator(vad_mute.begin()), std::make_move_iterator(vad_mute.end()));
      vad_mute.clear();
    }
    vad_data_ptr->insert(vad_data_ptr->end(),  std::make_move_iterator(data->begin()), std::make_move_iterator(data->end()));  
  } else {
    if (asr_final) {
      vad_stop = std::chrono::system_clock::now();
      auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(vad_stop - vad_pre).count();
      if (interval >= 1 * 1000) {
        asr_final = false;
        auto temp_ptr = std::make_shared<std::vector<double>>();
        std::lock_guard<std::mutex> lock(mutex);
        if (process_queue.size() >= kMaxQueueSize) {
          process_queue.pop();
        }
        process_queue.push(std::make_pair(temp_ptr, 1));
        cv.notify_one();
      }
    }
  }
  return;
}

void speech_engine::process(void) {
  try {
    std::string result_str;
    while (true) {
      {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return !process_queue.empty() || stop_flag; });
      }
      if (stop_flag) {
        break;
      }
      while (!process_queue.empty()) {
        auto data = process_queue.front().first;
        auto end_flag = process_queue.front().second;
        process_queue.pop();
        //std::vector<double> speech_segment;
        if (data->size() > 0) {
          auto start_time = std::chrono::system_clock::now();
          if (sense_voice_full_parallel(ctx, wparams, *data, data->size(), params.n_processors) != 0) {
              fprintf(stderr, "failed to process audio\n");
              continue;
          }
          auto end_time = std::chrono::system_clock::now();
          auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
          std::cout << "cost time :" << interval << " ms" << std::endl;
          std::string tmp_str;
          for (size_t i = 4; i < ctx->state->ids.size(); i++) {
            int id = ctx->state->ids[i];
            if (i > 0 && ctx->state->ids[i - 1] == ctx->state->ids[i])
                continue;
            if (id) {
              //printf("%s\n", ctx->vocab.id_to_token[id].c_str());
              tmp_str += ctx->vocab.id_to_token[id];
            }
          }
          for (auto& cmd : *v_cmd_word_) {
            if (tmp_str == cmd) {
              audio_cmd_cb_(tmp_str); //todo
              break;
            }

          }
          
          size_t pos = tmp_str.find(wakeup_name_, 0);
          if (pos != std::string::npos) {
            if (audio_asr_cb_) {
              audio_asr_cb_(wakeup_name_); //todo
            }
          }
          result_str += tmp_str + ",";
          std::cout << "result_str:" << result_str << std::endl;
        }
        if (end_flag) {
          if (audio_asr_cb_) {
            audio_asr_cb_(result_str); //todo
          }
          std::cout << "result_str:" << result_str << std::endl;
          result_str.clear();
        }

      }
    }
  } catch(...) {
    std::cout << "exception speech_engine::process"<<std::endl;
  }
  return;
}
