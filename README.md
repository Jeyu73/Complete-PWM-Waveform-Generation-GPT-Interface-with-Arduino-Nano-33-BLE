# Complete-PWM-Waveform-Generation-GPT-Interface-with-Arduino-Nano-33-BLE
This project uses the Arduino Nano 33 BLE Microcontroller to accept frequency and voltage inputs from parsed variables in a interface to create sine waveforms.
There are three components of this device: The user interface tooling, arduino firmware, and hardware integration.
 - I designed and implemented the entire VS Code communication system, including gradio interface, a custom serial-communication protocol, paramete validation, message formatting, and synchronization logic, and all supporting scripts. I also included custom updates to global variables that can be run outside the jupyter notebook code block, enabling future additions such as real time monitoring modules.
 - Given base PWM code software, I modified 80% of the code to include adaptive Adaptive duty-cycle computation, Custom waveform table construction, Phase interpolation logic Amplitude-scaling and runtime parameter adjustment, Integration with the VS Code communication layer, Acknowledgment (ACK) responses, and Artificial Amplitude signal scaling.
 - On the Hardware, I created the voltage divider and mechanical amplitude scaling pot and anaylzed various low passes (electrolytic VS Ceramic) with the arduino code.

### Accessing the VS Code Interface, Arduino IDE, and Voltage Divider Docs
 - VS Code Interface: [EE105Projectfinal.ipynb](EEFORPROJECTIDE/EE105Projectfinal.ipynb)
 - Arduino IDE: [EE105FinalArduinoIDE.ino](EEFORPROJECTIDE/EE105WORKINGWellHigh/EE105FinalArduinoIDE.ino)
 - Voltage Divider: [Voltage Divider.docx](EEFORPROJECTIDE/Voltage%20Divider.docx)

## Check out recognition for this project!
 - Recognition: [ProjectRecognition.jpeg](ProjectRecognition.jpeg)

## Project Demonstration Video Google Drive Link
 - https://drive.google.com/file/d/1l1Sn12_UAtLwedJB7oNcmAaokX7LYiFn/view?usp=sharing

## Project Attribution
All team members developed significantly to the project. This list highlights major contributions.
- **Project Report**: Created by Reina Mitra
- **LT Spice Debugging**: Debugged by Sohana Singh
- **Arduino PWM base**: Originally developed by Iris Ye
- **Hardware Gain Pot and Low Pass**: Developed by Sam Street

- **Arduino Modifications**: Phase interpolation, amplitude scaling, adaptive table size, Jeremy Yu, 2025
- **VS Code Extension & Serial Communication Program**: Developed by Jeremy Yu, 2025
- **Mechanical Amplitude Scaling Pot for Low Pass Bases**: Developed by Jeremy Yu, 2025

Repository Link:
https://github.com/Jeyu73/Complete-PWM-Waveform-Generation-GPT-Interface-with-Arduino-Nano-33-BLE.git
