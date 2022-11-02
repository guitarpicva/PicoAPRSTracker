Pico APRS Tracker Electronics Notes

I built this example on a breadboard.  The Pico with it's header pins soldered on were attached to the breadboard and then a Goouuu Tech GPS module was attached by wires to the Pico on it's 4 header pins (UART TX,UART RX,VCC,GND).  I bought an adapter for the antenna connector which is terminated in an SMA socket for use with an outdoor antenna.

The Pico UART1 TX is Pico pin 11, RX is pin 12.  

Pico pins 39 (+5V) and 38 (GND) were used to drive the +/- bus areas of the breadboard.

Pico pin 1 is UART0 TX and pin 2 is UART0 RX.  These two lines, along with VCC and GND are connected to a MAX32 based TTL to RS-232 converter module.  In my case the module also outputs to a D-Sub 9 plug for RS-232.  This is connected by an appropriate serial cable to a surplus Coastal Chipworks TNC-X as the KISS modem.  The TNC-X jumpers are configured to use it's DB-9 port for RS-232 and the modem is powered by +12V DC.

The TNC-X is connected to a Yaesu FT-857D via it's Data port (6-pin Mini DIN).  The radio is set for 1200 baud packet on 144.390 (US APRS channel).  Currently the beacon period is set to 2 minutes, and every two minutes the created APRS position packet with time-stamp and Course/Speed is sent to the TNC-X for transmission.