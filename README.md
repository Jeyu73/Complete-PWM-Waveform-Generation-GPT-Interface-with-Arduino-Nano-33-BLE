# Complete-PWM-Waveform-Generation-GPT-Interface-with-Arduino-Nano-33-BLE
Using jupyter notebook through VScode as well as Arduino IDE. This program uses the Arduino Nano 33 BLE Microcontroller to accept frequency and voltage inputs to create sine waveforms.
There are three components of this device: The user interface tooling, arduino firmware, and hardware integration.
 - I designed and implemented the entire VS Code communication system, including gradio interface, a custom serial-communication protocol, paramete validation, message formatting, and synchronization logic, and all supporting scripts. I also included custom updates to global variables that can be run outside the jupyter notebook code block, enabling future additions such as real time monitoring modules.
 - Given base PWM code software, I modified the code to include, adaptive Adaptive duty-cycle computation, Custom waveform table construction, Phase interpolation logic Amplitude-scaling and runtime parameter adjustment, and Integration with the VS Code communication layer
 - On the Hardware, I created the voltage divider and mechanical amplitude scaling pot and anaylzed various low passes (electrolytic VS Ceramic) with the arduino code.

### Accessing the VS Code Interface, Arduino IDE, and Voltage Divider Docs
 - VS Code Interface: [EE105Projectfinal.ipynb](EEFORPROJECTIDE/EE105Projectfinal.ipynb)
 - Arduino IDE: [EE105FinalArduinoIDE.ino](EEFORPROJECTIDE/EE105WORKINGWellHigh/EE105FinalArduinoIDE.ino)
 - Voltage Divider: [Voltage Divider.docx](EEFORPROJECTIDE/Voltage%20Divider.docx)

## Product Attribution
- **Arduino PWM base**: Originally developed by Iris Liu Ye
- **Hardware Gain Pot and low pass base**: Developed by Sam Street

- **Arduino modifications**: phase interpolation, amplitude scaling, adaptive table size, Jeremy Yu, 2025
- **VS Code extension & serial communication**: Fully developed by Jeremy Yu, 2025
- **Mechanical Amplitude Scaling Pot for low pass bases** Fully developed by Jeremy Yu, 2025

Repository Link:
https://github.com/Jeyu73/Complete-PWM-Waveform-Generation-GPT-Interface-with-Arduino-Nano-33-BLE.git
