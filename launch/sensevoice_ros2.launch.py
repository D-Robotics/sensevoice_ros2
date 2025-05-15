# Copyright (c) 2024，D-Robotics.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'micphone_name',
            default_value='plughw:0,0',
            description='audio capture id'),
        DeclareLaunchArgument(
            'asr_pub_topic_name',
            default_value='/llamacpp_prompt',
            description='hobot audio publish topic name'),
        DeclareLaunchArgument(
            'audio_asr_model',
            default_value='sense-voice-small-fp16.gguf',
            description='hobot audio asr model'),
        DeclareLaunchArgument(
            'push_wakeup',
            default_value='0',
            description='push wakeup_name in asr before'),
        DeclareLaunchArgument(
            'wakeup_name',
            default_value='你好',
            description='wakeup name'),
        # 启动音频采集pkg
        Node(
            package='sensevoice_ros2',
            executable='sensevoice_ros2',
            output='screen',
            parameters=[
                {"micphone_name": LaunchConfiguration('micphone_name')},
                {"asr_model": LaunchConfiguration('audio_asr_model')},
                {"push_wakeup": LaunchConfiguration('push_wakeup')},
                {"wakeup_name": LaunchConfiguration('wakeup_name')},
                {"asr_pub_topic_name": LaunchConfiguration(
                    'asr_pub_topic_name')}
            ],
            arguments=['--ros-args', '--log-level', 'warn']
        )
    ])
