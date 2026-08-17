#!/usr/bin/env bash
set +e
echo "=== whoami ==="; whoami
echo "=== pwd ==="; pwd
echo "=== pc_demo build/bin ==="; ls -la ~/pc_demo/build/bin 2>/dev/null || echo "NO build/bin"
echo "=== upstream src dir ==="; ls -d ~/SenseVoice.cpp 2>/dev/null && echo "HAS UPSTREAM" || echo "NO UPSTREAM"
echo "=== model in ~/SenseVoice.cpp ==="; ls -la ~/SenseVoice.cpp/models/sense-voice-small-fp16.gguf 2>/dev/null || echo "NO MODEL up"
echo "=== model in pc_demo third_party ==="; ls -la ~/pc_demo/third_party/SenseVoice.cpp/models/sense-voice-small-fp16.gguf 2>/dev/null || echo "NO MODEL tp"
echo "=== libasound ==="; dpkg -l 2>/dev/null | grep -i libasound || echo "no libasound2-dev"
echo "=== arecord ==="; which arecord || echo "no arecord"
echo "=== pulse tools ==="; which pulseaudio pactl 2>/dev/null || echo "no pulse tools"
echo "=== ALSA cards ==="; cat /proc/asound/cards 2>/dev/null || echo "no /proc/asound/cards"
echo "=== WSL_INTEROP / DISPLAY ==="; echo "DISPLAY=$DISPLAY"; echo "PULSE_SERVER=$PULSE_SERVER"; echo "WSL2_GUI_ENV=$WSL2_GUI_ENV"
