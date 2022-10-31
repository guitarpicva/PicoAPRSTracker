Pico APRS Tracker

A little project to demonstrate using the real time clock which is disciplined by a GPS receiver module managed over a UART connection.

A small class UIKISSUtils can wrap and unwrap data into/out of KISS frames.  A function to create a valid UI frame for APRS is also included.

Once the GPS position and time is updated on the Pico's Real Time Clock, it can be queried every second as GPS data is read and if a valid divisor of the current minute out of 60 minutes is reached, an APRS beacon packet is made and sent out of the MODEM UART.

This means that valid divisor minutes are 2,3,4,5,12,15,20,30 (in practice these are the most useful for APRS beacons).

Code will be added soon to adjust this divisor based on GPS provided speed in the GPRMC sentence of NMEA data.  The faster the device is traveling, the lower the divisor becomes with a floor of 2 minutes.  As the device slows it ramps up the interval divisor to a ceiling of 30 minutes.  Currently working out the appropriate inflection points for the divisor.