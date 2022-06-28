# Ball
![Image](https://raw.githubusercontent.com/dchwebb/Ball/main/pictures/ball_remote.jpg "icon")

Overview
--------

Ball is a Bluetooth remote controller for a Eurorack modular synthesiser. The remote controller is a sphere containing a PCB with an STM32WB35 BLE transmitter connected to an STM MEMS gyroscope. The remote communicates over Bluetooth Low Energy (BLE) with a base unit. The base unit is a standard Eurorack module that converts the three axis gyroscope information into three control voltages.

Project Status
--------------

Project is currently on hold following partially successful first prototype stage. Parts shortages of both microcontrollers and MEMS units are making further prototypes impossible to build. BLE functionality is partly working but connections are proving extremely unreliable probably as a result of incorrectly matched antennas. Proper RF matching would require significant expenditure in RF test equipment (VNA etc) making progress on the project impractical at this time.

Technical
---------
![Image](https://raw.githubusercontent.com/dchwebb/Ball/main/pictures/ball_base.jpg "icon")

Both the base unit and remote were initially developed on the STM32WB55 nucleo and dongle hardware. Lack of availability of the WB55 has required the use of STM32WB35 MCU in the initial prototypes.

Remote
------

The remote unit is designed to be powered from a Li-Po battery which is charged via a USB micro connection. Battery charging uses a MCP73831 Microchip Charge Management Controller. The battery can be isolated with a slide switch. A mosfet switches either the battery or USB power to a 2.8V linear regulator to power the MCU and gyroscope. A combined voltage divider passes the scaled battery voltage to an ADC in the MCU for battery level monitoring. The voltage divider also controls the inhibit line on the LDO to avoid drawing excessive power from the battery.

The 3-D sensing is carried out on an L3GD20 MEMS gyroscope from STM. Initial prototyping was carried out using an Adafruit module. This exposed the gyro over I2C. On the first remote prototype it was intended to use the gyro in SPI mode directly soldered to the PCB (though footprints to use the module were included as a fall-back). Unfortunately parts shortages meant that the gyro was unobtainable - the only one that could be found was an (apparently) faulty unit sourced from China so the SPI part of the circuit remains untested.

The USB charging port on the remote is also used for serial communication with a host computer for testing and configuration.

Base Unit
---------

The base unit also features an STM32WB35. This receives the 3 dimensional gyroscope data from the remote unit over BLE using the HID Joystick specification. (Theoretically this should mean the unit could also support other BLE game controllers but this has not been tested.)

The STM32WB does not contain any DACs so the gyroscope XYZ data is supplied as PWM data which is filtered and scaled to 0-5V. Three LEDs display the current level of each CV output. An additional blue LED showing BLE connection status is positioned behind a PCB Bluetooth 'window' - ie shines through the FR4 with no solder mask or copper.

Antennas and Matching Network
-----------------------------

Given that a VNA that operates at Bluetooth frequencies (2.4GHz) is prohibitively expensive the antenna matching network was designed using largely application notes and guesswork. The matching network consists of an 0402 PI network of capacitor/inductor/capacitor connected to an Ignion NN01_102 antenna.

Both units were visible but connection strength appeared highly variable and connections could not be sustained in testing.

![Image](https://raw.githubusercontent.com/dchwebb/Ball/main/pictures/ball_base_circuit.jpg "icon")