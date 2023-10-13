# Ball
![Image](https://raw.githubusercontent.com/dchwebb/Ball/main/pictures/ball_remote_closed.jpg "icon")

Overview
--------

Ball is a Bluetooth remote controller for a Eurorack modular synthesiser. The remote controller is a sphere containing a PCB with an STM32WB35 BLE transmitter connected to an STM MEMS gyroscope. The remote communicates over Bluetooth Low Energy (BLE) with a base unit. The base unit is a standard Eurorack module that converts the three axis gyroscope information into three control voltages.

Remote
------

![Image](https://raw.githubusercontent.com/dchwebb/Ball/main/pictures/ball_remote.jpg "icon")

The remote unit is designed around an STM32WB35 and powered from a Li-Po battery which is charged via a USB micro connection. Battery charging uses a MCP73831 Microchip Charge Management Controller. The battery can be isolated with a slide switch. A mosfet switches either the battery or USB power to a 2.8V linear regulator to power the MCU and gyroscope. A combined voltage divider passes the scaled battery voltage to an ADC in the MCU for battery level monitoring. The voltage divider also controls the inhibit line on the LDO to avoid drawing excessive power from the battery.

The 3-D sensing is carried out on an L3GD20 MEMS gyroscope from STM. This communicates with the MCU via full-duplex SPI. Initial prototyping was carried out using I2C on an Adafruit module and the pins for connecting to the module are fitted to the prototype board. The pins exposing the gyroscope ready and interrupt were bodged into the design via the (now unused) I2C pins. This allows waking from sleep mode via motion detection and readings to be taken from the gyroscope as soon as the ready interrupt fires.

![Image](https://raw.githubusercontent.com/dchwebb/Ball/main/pictures/ball_remote_open.jpg "icon")

The prototype Remote unit was fitted to a black sphere requisitioned from a child's toy.

[Schematic](https://raw.githubusercontent.com/dchwebb/Ball/main/ball_remote_schematic.pdf)

Base Unit
---------

![Image](https://raw.githubusercontent.com/dchwebb/Ball/main/pictures/ball_base.jpg "icon")

The base unit also features an STM32WB35. This receives the 3 dimensional gyroscope data from the remote unit over BLE using the HID Joystick specification. (Theoretically this should mean the unit could also support other BLE game controllers but this has not been tested.)

The STM32WB does not contain any DACs so the gyroscope XYZ data is supplied as PWM data which is filtered and scaled to 0-5V. Three LEDs display the current level of each CV output. An additional blue LED showing BLE connection status is positioned behind a PCB Bluetooth 'window' - ie shines through the FR4 with no solder mask or copper.

[Schematic](https://raw.githubusercontent.com/dchwebb/Ball/main/ball_base_schematic.pdf)

Antennas and Matching Network
-----------------------------

The antenna matching network was designed using largely application notes and guesswork. The matching network consists of an 0402 PI network of capacitor/inductor/capacitor connected to an Ignion NN01_102 antenna. Whilst connection strength could probably be improved if matched with a VNA, communications work well in testing.

![Image](https://raw.githubusercontent.com/dchwebb/Ball/main/pictures/ball_base_circuit.jpg "icon")

Power Management and Sleep
--------------------------

As the Remote unit is battery powered various systems are used to reduce power consumption:

When the unit is first powered or reset it begins fast advertising which allows for near-instant connections. After a configurable period has elapsed the unit switches to more infrequent advertising. After a further configurable period of inactivity the unit enters shutdown and can be restarted via the reset button.

Once a connection has been established to the base unit the remote will transmit at full speed (around 380Hz). If no motion is detected after a few seconds the unit will slow down the sampling rate to 190Hz and if no motion is detected after a few more seconds will enter sleep mode. The device can woken via motion (using the gyroscope's interrupt pin), or from the base unit (ie to request a battery level update). If woken from motion the sleep process will repeat once the unit is at rest, or if woken via RF will immediately return to sleep.

The Remote unit's USB peripheral is disabled unless a battery reading of 100% is registered (ie the unit is connected to USB and powered via VBUS). If USB is enabled current draw is around 2mA higher than usual. a USB serial command will also momentarily wake the unit if sleeping.

Measured current consumption of the remote (MCU and all other hardware) is measured as approximately:

- Advertising: 13mA
- Connected: 16mA
- Sleep: 10mA
- Shutdown: 0.4mA

USB Serial Console
------------------

Both the base and remote have USB serial consoles providing a range of diagnostic and configuration information:

![Image](https://raw.githubusercontent.com/dchwebb/Ball/main/pictures/remote_console.png "icon")

Development Software
--------------------

- Schematic and PCB design done in Kicad v6/7.
- Firmware developed in STM32CubeIDE v1.13.2.

Errata
------

Remote:

- SPI MOSI and MISO connections swapped
- Gyroscope Interrupt and Ready pins bodged onto unused I2C pins
- Mosfet Q1 source and drain pins swapped
- Battery charging and monitoring circuit could be improved (eg using a dedicated Battery Management IC such as TI's BQ24079QWRGTRQ1)
