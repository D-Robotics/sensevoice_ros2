#!/usr/bin/env bash
set +e
cd ~/SenseVoice.cpp
echo "=== sense_voice_context_params full ==="
grep -n "struct sense_voice_context_params" -A 40 sense-voice/csrc/common.h
echo "=== sense_voice_context struct ==="
grep -n "struct sense_voice_context" -A 30 sense-voice/csrc/common.h
echo "=== sense_voice_lang_id / sense_voice_full_default_params ==="
grep -n "sense_voice_lang_id\|sense_voice_full_default_params\|sense_voice_vocab\|struct sense_voice_state" sense-voice/csrc/common.h sense-voice/csrc/sense-voice.h
echo "=== SENSE_VOICE_SAMPLING_GREEDY define ==="
grep -rn "SENSE_VOICE_SAMPLING_GREEDY\|SENSE_VOICE_SAMPLING" sense-voice/csrc/*.h
