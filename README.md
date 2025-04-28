English| [简体中文](./README_cn.md)

# Introduction

D-Robotics Intelligent Voice Algorithm adopts local offline mode, subscribes to audio data for processing by BPU, and then publishes messages such as **command word **, and **Automatic Speech Recognition（ASR）**. The implementation of intelligent voice function corresponds to the **sensevoice_ros2** package of TogetheROS.Bot, which is suitable for the microphone array matched with the D-Robotics RDK.

After the intelligent voice sensevoice_ros2 package starts running, it will collect audio from the microphone array and send the collected audio data to the smart speech algorithm SDK module for intelligent processing. It outputs intelligent information such as wake-up events, command words, ASR results, etc. Wake-up events and command words are published as messages of type `audio_msg::msg::SmartAudioData`, and ASR results are published as messages of type `std_msgs::msg::String`.


# Usage Guide

## Preparation

Before experiencing, you need to meet the following basic requirements:

- D-Robotics RDK has installed the Ubuntu 20.04 system image provided by D-Robotics.
- The audio board is correctly connected to RDK X3.


## Installation

After starting RDK X3, connect to the robot via terminal SSH or VNC, copy and run the following commands on the RDK system to install related Nodes.

tros humble:
```bash
sudo apt update
sudo apt install -y tros-humble-hobot-audio
```

## Execution

The intelligent voice function supports ASR recognition after denoising the original audio. The default wake-up word and command word are defined in the directory of the intelligent voice function module as *config/hrsc/cmd_word.json*, which are as follows by default:

```json
{
    "cmd_word": [
        "地平线你好",
        "向前走",
        "向后退",
        "向左转",
        "向右转",
        "停止运动"
    ]
}
```

The word in `cmd_word` is command words that users can configure as needed. It is recommended to use Chinese for command words, preferably words that are easy to pronounce and with a length of 3 to 5 characters.

To run the hobot_audio package on the D-Robotics RDK board:

1. Configure the tros.b environment and start the application

    tros humble:
    ```shell
    # Configure the tros.b humble environment
    source /opt/tros/humble/setup.bash

    # Start the launch file
    ros2 launch hobot_audio hobot_audio.launch.py
    ```
2. Result

    The output information of the D-Robotics RDK board end terminal is as follows:

    ```text
    alsa_device_init, snd_pcm_open. handle((nil)), name(plughw:0,0), direct(1), mode(0)
    snd_pcm_open succeed. name(plughw:0,0), handle(0xaaaad1248290)
    Rate set to 16000Hz (requested 16000Hz)
    Buffer size range from 32 to 131072
    Period size range from 16 to 1024
    Requested period size 512 frames
    Periods = 4
    was set period_size = 512
    was set buffer_size = 2048
    alsa_device_init. hwparams(0xaaaad12484a0), swparams(0xaaaad124a7a0)
    ```

    The above log shows that the audio device initialization was successful, and the audio device was opened for audio capture.

    When a person successively says the command words"向前走" (Move forward), "向左转" (Turn left), "向右转" (Turn right), "向后退" (Move backward) near the microphone, the voice algorithm SDK outputs the recognition results after intelligent processing, as shown in the log:

    ```text
    cost time :769 ms
    [WARN] [1745810610.317172494] [sensevoice_ros2]: recv cmd word:向前走
    result_str:向前走,
    [WARN] [1745810610.479493615] [sensevoice_ros2]: asr msg:向前走,
    result_str:向前走,
    cost time :785 ms
    [WARN] [1745810614.078700989] [sensevoice_ros2]: recv cmd word:向左转
    result_str:向左转,
    [WARN] [1745810614.187793932] [sensevoice_ros2]: asr msg:向左转,
    result_str:向左转,
    cost time :761 ms
    [WARN] [1745810616.453310236] [sensevoice_ros2]: recv cmd word:向右转
    result_str:向右转,
    [WARN] [1745810616.587498515] [sensevoice_ros2]: asr msg:向右转,
    result_str:向右转,
    cost time :737 ms
    [WARN] [1745810618.700084757] [sensevoice_ros2]: recv cmd word:向后退
    result_str:向后退,
    [WARN] [1745810618.857481535] [sensevoice_ros2]: asr msg:向后退,
    result_str:向后退,
    ```


    The default topic name for intelligent voice messages published by hobot_audio is: */audio_smart*, and executing the `ros2 topic list` command in another terminal can query this topic information:

    ```bash
    $ ros2 topic list
    /audio_smart
    ```
    
    If ASR results publishing is enabled, the message topic published is: */audio_asr*, and the result of `ros2 topic list` is:

    ```bash
    $ ros2 topic list
    /audio_smart
    /audio_asr
    ```

# Interface Description

## Topics

| Name         | Message Type                                                                                                            | Description                                           |
| ------------ | ----------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------ |
| /audio_smart | [audio_msg/msg/SmartAudioData](https://github.com/D-Robotics/hobot_msgs/blob/develop/audio_msg/msg/SmartAudioData.msg) | Publish data and results processed by smart audio      |
| /audio_asr   | std_msgs/msg/String                                                                                                     | Publish ASR recognition results                        |

## Parameters

| Parameter Name      | Type        | Description            | Mandatory | Supported Configurations | Default Value |
| -------------------- | ----------- | ---------------------- | --------- | ----------------------- | ------------- |
| micphone_name          | std::string | Audio Capture ID| No        | Configure according to the actual situation | plughw:0,0    |
| audio_pub_topic_name | std::string | Audio smart frame publishing topic | No | Configure according to the actual situation | /audio_smart |
| asr_pub_topic_name   | std::string | ASR result publishing topic | No      | Configure according to the actual situation | /audio_asr   |


cmd_word.json

This configuration file configures command words for the speech intelligence analysis part. The default configuration file is as follows:

```json
{
    "cmd_word": [
        "向前走",
        "向后退",
        "向左转",
        "向右转",
        "停止运动"
    ]
}
```

# Frequently Asked Questions

1. Unable to open the audio device?

- Confirm if the audio device connection is normal
- Confirm if the audio device is correctly configured
- Confirm if the configuration file *config/audio_config.json* is set correctly
