# H1 Hydroponics Monitoring System
# H3This project is an embedded system designed to monitor critical water and environmental parameters for hydroponic cultivation. It is developed using the Espressif IoT Development Framework (ESP-IDF) and runs on the ESP32 microcontroller.

# H1 Project Overview
# H3 The system collects data from multiple sensors to provide real-time monitoring of the hydroponic environment. The current implementation focuses on acquiring and processing data from three specific sensors:

1. Water Temperature Sensor (DS18B20): Measures the water temperature using the OneWire protocol.

2. TDS Sensor: Measures the total dissolved solids in the water using the ESP32 internal ADC.

3. Air Temperature Sensor (BMP280): Measures the ambient air temperature using the I2C protocol.

# H1 Hardware Components
1. Microcontroller: ESP32 (DevKit)

2. Water Temp Sensor: DS18B20

3. Air Temp Sensor: BMP280

4. TDS Sensor: Analog TDS probe with signal conditioning

# H1 Software Architecture
1. The project is built on the ESP-IDF framework, utilizing the following principles:

2. Modular Initialization: Each sensor has a dedicated initialization function to configure the required hardware peripherals (I2C, OneWire, and ADC) independently.

3. Peripheral Management: The system utilizes the official ESP-IDF driver layer to handle communication protocols, ensuring stable and reliable data acquisition.

4. Data Processing: The code implements specific algorithms for sensor data compensation and conversion to ensure accurate readings.