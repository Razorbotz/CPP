from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='video_stream',
            namespace='',
            name='video_stream',
            executable='video_stream_node',
            parameters=[
                {"window_width": 100},
                {"window_height": 100},
                {"window_x": 100},
                {"window_y": 100},
            ]
        )
    ]
)
