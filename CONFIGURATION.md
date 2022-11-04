#How to configure the Pico APRS Tracker via USB

WARNING: Do not connect the USB cable to the Pico until you have read the entire document.

The code enables the USB as a stdio so debug trace and configuration can occur via the Pico's USB port and a serial terminal program.

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
!!!!!!!!!!!!!!!!!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!When the USB is connected ALTERNATE POWER SOURCES SHOULD NOT BE USED!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
See the RPi Pico documentation for power requirements and alternate ways to dual-power the device.

Connect the serial terminal to the Serial port that is created when the Pico is plugged into the computer.  The settings should be 115200 baud 8N1 with no flow control.

Once the USB is connected to the serial terminal (PuTTY, minicom, gtkterm, etc.) trace from the running code should appear in the terminal.  You should see incoming packets from your KISS modem that are decoded for content and other information from the running program.  As the program matures, much of this information will be stopped in order to save resources for the running program.

There are currently two commands read from the terminal by the program.
1) "READCONFIG|"
2) "WRITECONFIG|<source>|<digi1>|<digi1>|<comment>|<interval in min.>|<APRS symbol character>"

i.e WRITECONFIG|AB4MW-12|WIDE1-1|WIDE2-1|github guitarpicva/PicoAPRSTracker|4|j

When the READCONFIG| command is sent, the Pico will read it's configuration values which have been stored in the Flash memory of the device and apply them to the running program.

When the WRITECONFIG|... command is sent, the Pico will parse the rest of the line and write that to the Flash memory of the device, then immediately read those values back and apply them to the running program.  

!!!!!!!!!!!!!!!!!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
The configuration code is in early stages with little in the way of sanity or error checking.  
!!!!!!!!!!!!!!!!!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
Please be extra careful how the string is created and  note that ALL fields are required!
!!!!!!!!!!!!!!!!!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!