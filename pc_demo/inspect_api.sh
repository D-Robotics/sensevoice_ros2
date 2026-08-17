#!/usr/bin/env bash
set +e
cd ~/SenseVoice.cpp
H=sense-voice/csrc/sense-voice.h
echo "=== sense_voice_context_params struct ==="
grep -n "struct sense_voice_context_params" -A 30 $H
echo "=== sense_voice_full_params struct ==="
grep -n "struct sense_voice_full_params" -A 20 sense-voice/csrc/common.h
echo "=== key API declarations ==="
grep -n "SENSEVOICE_API.*sense_voice_small_init_from_file_with_params\|SENSEVOICE_API.*sense_voice_full_parallel\|SENSEVOICE_API.*sense_voice_free\|SENSEVOICE_API.*silero_vad" $H
echo "=== vocab / ids fields in context ==="
grep -n "id_to_token\|struct sense_voice_vocab\|ids\b\|->state" sense-voice/csrc/common.h | head -20
