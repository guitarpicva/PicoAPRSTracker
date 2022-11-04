Pico APRS Tracker

A Pico SDK project to demonstrate using the real time clock which is disciplined by a GPS receiver module managed over a UART connection. It also demonstrates using the second core
of the Pico to run the GPS NMEA sentence evaluation and RTC updates.

This tracker keeps it's position by reading the GPS position and time data via UART0.  A KISS Packet modem is connected to the Pico via UART1 (along with an appropriate converter module to RS-232 as required).

Pico APRS Tracker is shared in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

A small class UIKISSUtils can wrap and unwrap data into/out of KISS frames.  A function to create a valid UI frame for APRS is also included.  The function unwrapUIFrame is used to print information about received packets from the KISS modem on UART1.

Once the GPS position and time is updated on the Pico's Real Time Clock, it is queried every second as GPS data is read and if a valid divisor of the current minute out of 60 minutes is reached, an APRS beacon packet is made and sent out of the MODEM UART.

This means that valid divisor minutes are 2,3,4,5,12,15,20,30 (in practice these are the most useful for APRS beacons).

Code will be added soon to adjust this divisor based on GPS provided speed in the GPRMC sentence of NMEA data.  The faster the device is traveling, the lower the divisor becomes with a floor of 2 minutes.  As the device slows it ramps up the interval divisor to a ceiling of 30 minutes.  Currently working out the appropriate inflection points for the divisor. 

See the ELECTRONICS.md file for hints about how to build the example implementation and connect it to a KISS Packet modem and radio.