#!/usr/bin/env python3
import sys
import rclpy
from rclpy.node import Node
from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from wave_control_msgs.msg import WaveCommand
from wave_control_msgs.srv import WaveControl

class WaveControlGUI(QMainWindow):
    def __init__(self, node):
        super().__init__()
        self.node = node
        self.init_ui()
        
    def init_ui(self):
        self.setWindowTitle('Wave Control System')
        self.setGeometry(100, 100, 400, 300)
        
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)
        
        # Control buttons
        self.start_btn = QPushButton('Start Waving')
        self.start_btn.clicked.connect(self.start_waving)
        layout.addWidget(self.start_btn)
        
        # Wave part selection
        self.wave_part_combo = QComboBox()
        self.wave_part_combo.addItems(['right_arm', 'left_arm', 'both'])
        layout.addWidget(QLabel('Wave Part:'))
        layout.addWidget(self.wave_part_combo)
        
        # Amplitude slider
        self.amplitude_slider = QSlider(Qt.Horizontal)
        self.amplitude_slider.setRange(10, 90)
        self.amplitude_slider.setValue(45)
        self.amplitude_label = QLabel('Amplitude: 45°')
        self.amplitude_slider.valueChanged.connect(
            lambda v: self.amplitude_label.setText(f'Amplitude: {v}°')
        )
        layout.addWidget(self.amplitude_label)
        layout.addWidget(self.amplitude_slider)
        
        # Frequency slider
        self.frequency_slider = QSlider(Qt.Horizontal)
        self.frequency_slider.setRange(1, 50)
        self.frequency_slider.setValue(10)
        self.frequency_label = QLabel('Frequency: 1.0 Hz')
        self.frequency_slider.valueChanged.connect(
            lambda v: self.frequency_label.setText(f'Frequency: {v/10:.1f} Hz')
        )
        layout.addWidget(self.frequency_label)
        layout.addWidget(self.frequency_slider)
        
        # Change and Stop buttons
        self.change_btn = QPushButton('Change State')
        self.change_btn.clicked.connect(self.change_state)
        layout.addWidget(self.change_btn)
        
        self.stop_btn = QPushButton('Stop Waving')
        self.stop_btn.clicked.connect(self.stop_waving)
        layout.addWidget(self.stop_btn)
        
        # Status label
        self.status_label = QLabel('Status: Ready')
        layout.addWidget(self.status_label)
        
    def start_waving(self):
        self.send_command('wave_hello')
        
    def change_state(self):
        self.send_command('change_state')
        
    def stop_waving(self):
        self.send_command('stop')
        
    def send_command(self, operation_type):
        request = WaveControl.Request()
        request.command = WaveCommand()
        request.command.robot_id = "ROBOT_001"
        request.command.operation_type = operation_type
        request.command.wave_part = self.wave_part_combo.currentText()
        request.command.amplitude = float(self.amplitude_slider.value())
        request.command.frequency = self.frequency_slider.value() / 10.0
        
        future = self.node.client.call_async(request)
        future.add_done_callback(self.handle_response)
        
    def handle_response(self, future):
        response = future.result()
        if response.success:
            self.status_label.setText(f'Status: {response.message}')
        else:
            self.status_label.setText(f'Error: {response.message}')

class GUINode(Node):
    def __init__(self):
        super().__init__('wave_gui_node')
        self.client = self.create_client(WaveControl, 'wave_control')

def main(args=None):
    rclpy.init(args=args)
    
    app = QApplication(sys.argv)
    node = GUINode()
    
    gui = WaveControlGUI(node)
    gui.show()
    
    # Run ROS2 in a separate thread
    from threading import Thread
    ros_thread = Thread(target=lambda: rclpy.spin(node))
    ros_thread.daemon = True
    ros_thread.start()
    
    sys.exit(app.exec_())

if __name__ == '__main__':
    main()
