English| [简体中文](./README_cn.md)

# Introduction

D-Robotics Intelligent Voice Algorithm adopts local offline mode, subscribes to audio data for processing by BPU, and then publishes messages such as **command word wake-up**, **Direction Of Arrival（DOA）**, and **Automatic Speech Recognition（ASR）**. The implementation of intelligent voice function corresponds to the **hobot_audio** package of TogetheROS.Bot, which is suitable for the microphone array matched with the D-Robotics RDK.

After the intelligent voice hobot_audio package starts running, it will collect audio from the microphone array and send the collected audio data to the smart speech algorithm SDK module for intelligent processing. It outputs intelligent information such as wake-up events, command words, ASR results, etc. Wake-up events and command words are published as messages of type `audio_msg::msg::SmartAudioData`, and ASR results are published as messages of type `std_msgs::msg::String`.

The specific process is as shown in the following figure:

![hobot_audio](./imgs/hobot_audio.jpg)

Application Scenarios: The intelligent voice algorithm can recognize wake-up words and custom command words in audio, interpret the voice content into corresponding instructions or convert it into text, enabling functions such as voice control and voice translation. It is mainly used in smart home, smart cabin, smart wearable devices, and other fields.

# Bill of Materials

| Robot Name   | Manufacturer | Reference Link                                                  |
| :----------- | ------------ | --------------------------------------------------------------- |
| RDK X3       | Multiple     | [Click to Jump](https://developer.D-Robotics.cc/rdkx3)              |
| 4mic Microphone Array   | Waveshare Electronics | [Click to Jump](https://www.waveshare.net/shop/Audio-Driver-HAT.htm) |
| 2mic Microphone Array   | Waveshare Electronics | [Click to Jump](https://www.waveshare.net/shop/WM8960-Audio-HAT.htm) |

# Usage Guide

## Preparation

Before experiencing, you need to meet the following basic requirements:

- D-Robotics RDK has installed the Ubuntu 20.04 system image provided by D-Robotics.
- The audio board is correctly connected to RDK X3.

Connection Steps:

1. Connect the microphone array to the 40PIN GPIO interface of D-Robotics RDK X3. The physical appearance after connection is as follows:
    - 4mic Microphone Array

    ![circle_mic_full](./imgs/circle_mic_full.png)

    - 2mic Microphone Array

    ![circle_mic_full](./imgs/2mic_full.jpg)

2. Configure the microphone array, refer to RDK User Manual [Audio Adapter](https://developer.D-Robotics.cc/documents_rdk/hardware_development/rdk_x3/audio_board) section.

## Installation

After starting RDK X3, connect to the robot via terminal SSH or VNC, copy and run the following commands on the RDK system to install related Nodes.

tros foxy:
```bash
sudo apt update
sudo apt install -y tros-hobot-audio
```

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

The first word in `cmd_word` is the wake-up word, followed by command words that users can configure as needed. Changing the wake-up word may result in different effects from the default wake-up word command. It is recommended to use Chinese for wake-up words and command words, preferably words that are easy to pronounce and with a length of 3 to 5 characters.

In addition, the intelligent voice function supports output of DOA (Direction Of Arrival) angle information, measured in degrees, for a circular microphone array with a range of 0° to 360°. Currently, only the 4-microphone array board supports DOA information output, while the 2-microphone board does not support it.

The relative positional relationship of angles is strongly correlated with the microphone installation position. The following diagram illustrates the DOA angles for a 4-microphone circular array:

![doa_circle](./imgs/doa_circle.jpg)

To run the hobot_audio package on the D-Robotics RDK board:

1. Copy the configuration files

   tros foxy:
    ```shell
    # Copy the configuration files needed for running examples from the installation path of tros.b. This step can be ignored if already copied before.
    cp -r /opt/tros/${TROS_DISTRO}/lib/hobot_audio/config/ .
    ```

   tros humble:
    ```shell
    source /opt/tros/humble/setup.bash
    # Copy the configuration files needed for running examples from the installation path of tros.b. This step can be ignored if already copied before.
    cp -r /opt/tros/${TROS_DISTRO}/lib/hobot_audio/config/ .
    ```

2. Verify the configuration file *config/audio_config.json* 

    The fields to verify include:

    - `micphone_name`, configure the audio device number, default is "hw:0,0". If there are no other audio devices connected when loading the audio driver, there is no need to modify this field. If there are other audio devices connected when loading the audio driver, such as a USB microphone or a USB camera with microphone function, then this field needs to be modified to the corresponding device number. For example, "hw:0,0" represents audio device Card0 Device0.
    - `micphone_chn`, configure the number of channels supported by the audio board. Set this field to `8` for a 4-microphone board, and `2` for a 2-microphone board.
    - `asr_mode`, configure whether to publish ASR (Automatic Speech Recognition) results, default value is `0`, indicating no ASR result publication. For publishing ASR results, this field needs to be changed to `1` or `2`, where `1` means performing ASR recognition once after wake-up and publishing the result, and `2` means continuously performing ASR recognition and publishing results.
    - `asr_channel`, channel number used for ASR recognition, set to `3` for a 4-microphone board, and `1` for a 2-microphone board.

3. Configure the tros.b environment and start the application

    tros foxy:
    ```shell
    # Configure the tros.b environment
    source /opt/tros/setup.bash

    # Suppress debug print information
    export GLOG_minloglevel=3

    # Start the launch file
    ros2 launch hobot_audio hobot_audio.launch.py
    ```

    tros humble:
    ```shell
    # Configure the tros.b humble environment
    source /opt/tros/humble/setup.bash

    # Suppress debug print information
    export GLOG_minloglevel=3

    # Start the launch file
    ros2 launch hobot_audio hobot_audio.launch.py
    ```
4. Result

    The output information of the D-Robotics RDK board end terminal is as follows:

    ```text
    alsa_device_init, snd_pcm_open. handle((nil)), name(hw:0,0), direct(1), mode(0)
    snd_pcm_open succeed. name(hw:0,0), handle(0x557d6e4d00)
    Rate set to 16000Hz (requested 16000Hz)
    Buffer size range from 16 to 20480
    Period size range from 16 to 10240
    Requested period size 512 frames
    Periods = 4
    was set period_size = 512
    was set buffer_size = 2048
    alsa_device_init. hwparams(0x557d6e4fa0), swparams(0x557d6e5210)
    ```

    The above log shows that the audio device initialization was successful, and the audio device was opened for audio capture.

    When a person successively says the command words "地平线你好", "向前走" (Move forward), "向左转" (Turn left), "向右转" (Turn right), "向后退" (Move backward) near the microphone, the voice algorithm SDK outputs the recognition results after intelligent processing, as shown in the log:

    ```text
    recv hrsc sdk event wakeup success, wkp count is 1
    [WARN] [1657869437.600230208] [hobot_audio]: recv event:0
    recv hrsc sdk doa data: 100
    recv hrsc sdk command data: 向前走 (Move forward)
    [WARN] [1657869443.870029101] [hobot_audio]: recv cmd word:向前走 (Move forward)
    recv hrsc sdk doa data: 110
    recv hrsc sdk command data: 向左转 (Turn left)
    [WARN] [1657869447.623147766] [hobot_audio]: recv cmd word:向左转 (Turn left)
    recv hrsc sdk doa data: 100
    recv hrsc sdk command data: 向右转 (Turn right)
    [WARN] [1657869449.865822772] [hobot_audio]: recv cmd word:向右转 (Turn right)
    recv hrsc sdk doa data: 110
    recv hrsc sdk command data: 向后退 (Move backward)
    [WARN] [1657869452.313969277] [hobot_audio]: recv cmd word:向后退 (Move backward)
    ```

    The log shows that the voice commands "Move forward," "Turn left," "Turn right," and "Move backward" were recognized, and the angle information of Direction of Arrival (DOA) is output, such as the field "recv hrsc sdk doa data: 110" indicating a DOA angle of 110 degrees.

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
| config_path          | std::string | Configuration file path| No        | Configure according to the actual situation | ./config    |
| audio_pub_topic_name | std::string | Audio smart frame publishing topic | No | Configure according to the actual situation | /audio_smart |
| asr_pub_topic_name   | std::string | ASR result publishing topic | No      | Configure according to the actual situation | /audio_asr   |

Explanation of parameters in audio_config.json file:

| Parameter Name      | Type   | Explanation                                  | Mandatory | Supported Configurations | Default Value |
| -------------------- | ------ | ------------------------------------------- | --------- | ------------------------ | ------------- |
| micphone_enable      | int    | Whether to enable the microphone             | Yes       | 0/1                      | 1             |
| micphone_name        | string | Microphone device number                     | No        | Configure according to the actual situation | "hw:0,0"     |
| micphone_rate        | int    | Microphone sampling rate                     | No        | 16000                    | 16000         |
| micphone_chn         | int    | Number of microphone channels                | Yes       | 8/2                      | 8             |
| micphone_buffer_time | int    | Circular buffer length in microseconds       | No        | Configure according to the actual situation | 0             |
| micphone_nperiods    | int    | Period time in microseconds                   | No       | 4                        | 4             |
| micphone_period_size | int    | Audio packet size                           | No        | Configure according to the actual situation | 512           |
| voip_mode            | int    | Whether it is in voip mode, if configured as voip mode, it publishes noise-reduced audio and does not support ASR recognition function. Noise reduction and ASR recognition modes are mutually exclusive. | Yes | 0/1 | 0 |
| mic_type             | int    | Microphone array type                        | No        | 0/1, 0: Circular microphone array, 1: Linear microphone array | 0 |
| asr_mode             | int    | ASR mode                                     | No        | 0/1/2, 0: Do not output asr results, 1: Output ASR recognition results once after wake-up, 2: Output ASR results continuously | 0 |
| asr_channel          | int    | ASR channel selection                        | No        | 4microphone board: 0/1/2/3; 2microphone board: 0/1 | 3 |
| save_audio           | int    | Whether to save audio data, including the original audio collected by the microphone and the noise-reduced audio output by the algorithm in voip mode. Audio is saved in the current directory where the program is running by default. | No | 0/1 | 0 |

cmd_word.json

This configuration file configures wake-up words and command words for the speech intelligence analysis part. The first item in the configuration file is the wake-up word, followed by command words. The default configuration file is as follows:

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

# Frequently Asked Questions

1. Unable to open the audio device?

- Confirm if the audio device connection is normal
- Confirm if the audio device is correctly configured
- Confirm if the configuration file *config/audio_config.json* is set correctly
