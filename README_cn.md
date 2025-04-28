[English](./README.md) | 简体中文

# 功能介绍

智能语音算法采用SenseVoiceGGUF的算法，订阅音频数据后送给sensevoicegguf模型处理，然后发布**命令词识别**、**语音ASR识别结果**等消息。智能语音功能的实现对应于TogetheROS.Bot的**sensevoice_ros2** package，适用于RDK配套的麦克风阵列。

应用场景：智能语音算法能够识别音频的语音内容解读为对应指令或转化为文字，可实现语音控制以及语音翻译等功能，主要应用于智能家居、智能座舱、智能穿戴设备等领域。

# 使用方法

## 准备工作

在体验之前，需要具备以下基本条件：

- RDK已烧录好Ubuntu 20.04系统镜像
- 音频板正确连接到RDK X3，RDK X5的3.5mm的耳麦接口。
- 或者USB音响正确连接到RDK X3，RDK X5的usb接口。


## 安装功能包

启动RDK X3，DK X5后，通过终端SSH或者VNC连接机器人，复制如下命令在RDK的系统上运行，完成相关Node的安装。

tros humble 版本
```bash
sudo apt update
sudo apt install -y tros-humble-sensevoice-ros2
```

## 运行智能语音程序

智能语音功能支持对原始音频进行ASR识别，默认命令词定义在智能语音功能模块目录下 *config/cmd_word.json* 文件，默认为：

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

`cmd_word`为命令词，这些词用户都可以根据需要配置。推荐命令词使用中文，最好是朗朗上口的词语，且词语长度推荐使用3~5个字。

RDK板端运行sensevoice_ros2 package：

1. 配置tros.b环境和启动应用

    tros humble 版本
    ```shell
    # 配置tros.b humble环境
    source /opt/tros/humble/setup.bash

    #启动launch文件
    ros2 launch sensevoice_ros2 sensevoice_ros2.launch.py micphone_name:="plughw:0,0"
    ```

2. 结果分析

    RDK板端运行终端输出如下信息：

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

    以上log显示，音频设备初始化成功，并且打开了音频设备，可正常采集音频。

    当人依次在麦克风旁边说出“向前走”、“向左转”、“向右转”、“向后退”命令词，语音算法sdk经过智能处理后输出识别结果，log显示如下：

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


    hobot_audio默认发布的智能语音消息话题名为：*/audio_smart*，在另一个终端执行使用`ros2 topic list`命令可以查询到此topic信息：

    ```bash
    $ ros2 topic list
    /audio_smart
    ```

    若开启发布ASR结果，发布消息话题为：*/audio_asr*，`ros2 topic list`结果为：

    ```bash
    $ ros2 topic list
    /audio_smart
    /audio_asr
    ```

# 接口说明

## 话题

| 名称         | 消息类型                                                                                                               | 说明                               |
| ------------ | ---------------------------------------------------------------------------------------------------------------------- | ---------------------------------- |
| /audio_smart | [audio_msg/msg/SmartAudioData](https://github.com/D-Robotics/hobot_msgs/blob/develop/audio_msg/msg/SmartAudioData.msg) | 发布智能语音处理后的数据和智能结果 |
| /audio_asr   | std_msgs/msg/String                                                                                                    | 发布ASR识别结果                    |

## 参数

| 参数名               | 类型        | 解释               | 是否必须 | 支持的配置       | 默认值       |
| -------------------- | ----------- | ------------------ | -------- | ---------------- | ------------ |
| micphone_name          | std::string | 语音采集设备       | 否       | 根据实际情况配置 | plughw:0,0     |
| audio_pub_topic_name | std::string | 音频智能帧发布话题 | 否       | 根据实际情况配置 | /audio_smart |
| asr_pub_topic_name   | std::string | ASR结果发布话题    | 否       | 根据实际情况配置 | /audio_asr   |


cmd_word.json

此配置文件配置语音智能分析部分的命令词。默认配置文件配置如下：

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

# 常见问题

1. 无法打开音频设备？

- 确认音频设备连接是否正常
- 确认是否正确配置音频设备
- 确认配置文件 *config/audio_config.json* 设置正确
