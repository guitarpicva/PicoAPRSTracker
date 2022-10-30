#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/rtc.h"
#include "pico/multicore.h"
#include <string>
#include <vector>
#include "ez_cpp_utils.h"

#define MODEM uart1 
#define GPS uart0
  
#define UART_MODEM_TX_PIN 8u
#define UART_MODEM_RX_PIN 9u
#define MODEM_BAUD 57600 // for Nino Modem, fixed at 57600

#define UART_GPS_TX_PIN 0u
#define UART_GPS_RX_PIN 1u
#define GPS_BAUD 9600 // for module on hand, most new ones are 9600
// struct to hold the position information
// from a GPRMC sentence
typedef struct {
        std::string fix;
        std::string lat;
        std::string latdir;
        std::string lon;
        std::string londir;
        std::string speed;
        std::string course;
} position_t;

bool b_rtcUpdated = false; // if the clock has been updated at start (of course not!)
bool b_fullcmd = false; // used by input queue handling
bool b_curr_chan = false; // LO channel by default
// Dummy start time for the RTC the struct is 8 bytes in length
datetime_t t = {
        .year  = 2022,
        .month = 06,
        .day   = 10,
        .dotw  = 0, // 0 is Sunday
        .hour  = 16,
        .min   = 9,
        .sec   = 0
};

position_t p = {
    .fix = "V",
    .lat = "36.5",
    .latdir = "N",
    .lon = "77.67",
    .londir = "W",
    .speed = "0.0",
    .course = "0.0"
};

std::string outq;  // output byte queue to the MODEM UART
std::string reply; // input byte queue from the MODEM UART
std::string gpsdata; // the buffer for the function running on core1

void loadOutQueue(std::string toSend, bool truncate) {
  //printf("%s", toSend.data());
  // add bytes to the outq for transmission to the MODEM
  // next time thru the loop
  if(truncate) {
    outq.clear();
    //Serial.println("outq is empty!");
  }
  outq += toSend;
}

/* changed to ez_cpp_utils.h instead
// split on a single char value, keeping empty values as tempty std::string
static void split(std::string str, std::vector<std::string> &token_v, const char delim)
{
  size_t start = str.find_first_not_of(delim), end=start;
  while (start != std::string::npos && end != std::string::npos){
      // Find next occurence of delimiter
      end = str.find(delim, start);
      // Push back the token found into vector
      token_v.push_back(str.substr(start, end - start));
      // Skip all occurences of the delimiter to find new start
      start = end + 1;
  }
} */

void handleGPSData() {
  gpsdata.clear();
  b_rtcUpdated = false; // initialize so gets set first time thru
  bool newData = false;
  //bool b_59Updated = false; // marks 59 minute updated so only once
  uint32_t start = 0;
  //printf("\n\nb_rtcUpdated:%d newData:%d\n\n", b_rtcUpdated, newData);
  while(true) { 
    // now let's see what the GPS module has to say
    newData = false;
    // For one second we parse GPS data and report some key values
    //unsigned long start = clock();
    start = to_ms_since_boot(get_absolute_time());
    //printf("%d\n", start);
  
    while ((to_ms_since_boot(get_absolute_time()) - start) < 1000)
    {
      if(uart_is_readable(GPS)) {
        //printf("READ GPS\n");
        gpsdata.push_back(uart_getc(GPS));
        if(!newData) newData = true;  
      }
    } // end read for one second
    if (newData)
    {
      //printf("%s\r", gpsdata.c_str());      // ok works
      size_t gprmc = gpsdata.find("$GPRMC"); // start of the gprmc sentence
      if(gprmc < 0) {gpsdata.clear(); b_rtcUpdated = false; continue;} // continue on missing sentence
      size_t endex = gpsdata.find("\r\n", gprmc); // end of the gprmc sentence
      
      std::string gprmcsentence = gpsdata.substr(gprmc, endex - gprmc);
      //printf("%s\n", gprmcsentence.c_str());
      std::vector<std::string> vic;
      split(gprmcsentence, vic, ',');
       //for(int i = 0; i < vic.size(); i++) {
      //    printf("%d: %s\n", i, (vic[i]).c_str());
      //  }
      //  printf("\n");
      gpsdata.clear(); // for the next iteration
      //if(vic[2].compare(std::string("A")) != 0) {b_rtcUpdated = false; continue;}  // no fix yet      this is done below now
      if(gprmcsentence.length() < 25) {b_rtcUpdated = false; continue;} // no fix yet so just loop
      size_t star = gprmcsentence.length() - 3; // work back from start of end 3 positions 
      std::string s_crc = gprmcsentence.substr(star+1, 2);
      //printf("s_crc: %s\n", s_crc.c_str());
      uint8_t g_crc = (uint8_t)std::stoi(s_crc, nullptr, 16);
      uint8_t crc = 0;
      for(int i = 1; i < star; i++)
      {
        // this adds up the crc
          crc ^= (uint8_t) gprmcsentence.at(i);
      }
      bool check = (crc == g_crc);
      //$GPRMC,220516,A,5133.82,N,00042.24,W,173.8,231.8,130694,004.2,W*70
      //         1    2    3    4    5     6    7    8      9     10  11 12
      if(check) {
        //printf("%s CRC:%X\r", gprmcsentence.c_str(), crc);      // OK! that works
        t.hour  = (int8_t)std::stoi(vic[1].substr(0, 2));
        t.min   = (int8_t)std::stoi(vic[1].substr(2, 2));
        t.sec   = (int8_t)std::stoi(vic[1].substr(4, 2));
        t.day   = (int8_t)std::stoi(vic[9].substr(0, 2)); // day of month
        t.month = (int8_t)std::stoi(vic[9].substr(2, 2)); // month of year
        t.year  = (int16_t)std::stoi(vic[9].substr(4, 2)) + 2000; // year up to 4095
        //printf("GPS Time: %4d-%02d-%02d %02d:%02d:%02d\r",t.year, t.month, t.day, t.hour, t.min, t.sec);     
        p.fix = vic[2];
        p.lat = vic[3].substr(0, 7); // for APRS format
        p.latdir = vic[4];
        p.lon = vic[5].substr(0,8); // for APRS format
        p.londir = vic[6];
        p.speed = vic[7];
        p.course = vic[8];
        printf("\rGPS Fix:%s Lat:%s%s Lon:%s%s Speed:%s Course:%s", p.fix.c_str(),p.lat.c_str(),p.latdir.c_str(),p.lon.c_str(),p.londir.c_str(),p.speed.c_str(),p.course.c_str());
        // update rtc if necessary
        if(p.fix == "A" && (b_rtcUpdated == false)) { // we have a fix and the clock is stale
          // update Pico RTC clock
          b_rtcUpdated = rtc_set_datetime(&t);
          //printf("RTCDT addr: %x", rtcdt);
          if(b_rtcUpdated) {
            // printf("Write RTC I2C: %d\n", i2c_write_blocking(i2c_default, GCA_ADDR, i2csend, 8, false));
            // sleep_ms(5); // ??
            printf("\nUpdated RTC: %4d-%02d-%02d %02d:%02d:%02d %d\n", t.year, t.month, t.day, t.hour, t.min, t.sec, b_rtcUpdated);
          }
        }
        // update the RTC at 59 past each hour
        if(t.min == 59) {
          if(!b_59Updated) {
            b_59Updated = true;
            b_rtcUpdated = rtc_set_datetime(&t);
            printf("\nUpdated RTC: %4d-%02d-%02d %02d:%02d:%02d %d\n", t.year, t.month, t.day, t.hour, t.min, t.sec, b_rtcUpdated);
          }
        }
        else {
          if(b_59Updated) b_59Updated = false; // reset it at minute zero or thereafter
        }
      } // otherwise no data or CRC bad so just keep looping
    }
  } // end WHILE
}

void loop() {
  //printf("Looping...\r");
  /*
  // Check GPIO 2 to possibly change channel
  if(gpio_get(SET_CHANNEL) != b_curr_chan) { 
    b_curr_chan = gpio_get(SET_CHANNEL);
    if(b_curr_chan)
      setFreq(std::string("ALERT HI"));  
    else
      setFreq(std::string("ALERT LO"));  
    printf("\nSet ALERT channel... %s", gpio_get(SET_CHANNEL)?"HI":"LO");    
  } */
  // Loop through checking the MODEM serial first
  // For HFA we only care about the minute of hour value from the GPS
  b_fullcmd = false;
  char inbyte='x';
  reply.clear();
  while(uart_is_readable(MODEM)) {
    //printf("READ MODEM");
    // read the byte, store it and look out for "\r" (0x0D)
    inbyte = uart_getc(MODEM);
    //printf((char *)&inbyte);
    reply.push_back(inbyte);
    if(inbyte == 0x0D) {
      inbyte = uart_getc(MODEM);  // we don't need the trailing line feed so just get it out of the way
      b_fullcmd = true;
      break; // change this to a function call to handle the full command
    }      
  }
  // post-process the input buffer from the MODEM
  if(b_fullcmd) { // should be a fully formed CODAN reply followed by \r\n (usually, otherwise only \r)
    // here we process the full command from the MODEM
    if(reply.length() > 2)
      printf("KISS Reply: %s\n", reply.c_str());
    //printf(reply.data());
  }
  
  // Now send commands to the MODEM if necessary
  if(outq.length() > 0){
    printf("\nKISS Command: %s\n", outq.c_str());
    for(int i = 0; i < outq.length(); i++) {
      uart_putc_raw(MODEM, outq.at(i));
      //printf("%02x ", (uint8_t)outq.at(i));
    }
    //uart_write_blocking(MODEM, (uint8_t*)outq.data(), outq.length());       
    outq.clear(); // clear the outq for next loop
  }    
}

int main() {
  stdio_init_all();
  sleep_ms(1000); // give it a sec to catch up before printf's begin
  // startup stdio on the USB port for trace
  stdio_usb_init();
  while(!stdio_usb_connected()){;}
  printf("USB Connected...");
  // Start the RTC
  rtc_init();
  rtc_set_datetime(&t); // dummy DTG defined at the top!
  // Set up the GPS UART interface on uart0
  printf("Set up GPS uart connection at %d\n", uart_init(GPS, GPS_BAUD));  //8N1 No flow control std. it seems  
  printf("GPS uart0 Enabled? %d\n", uart_is_enabled(GPS));  
  // GPS module Pins 0, 1
  gpio_set_function(UART_GPS_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_GPS_RX_PIN, GPIO_FUNC_UART);

  // Now the MODEM uart1
  // Set up the MODEM UART interface on uart1
  printf("Set up MODEM uart connection at %d and send KISS setup commands...\n", uart_init(MODEM, MODEM_BAUD));
  printf("MODEM uart1 Enabled? %d\n", uart_is_enabled(uart1));  
  // turn off RTS/CTS flow control
  uart_set_hw_flow(MODEM, false, false); // defaults not specified 
  uart_set_format(MODEM, 8, 1, UART_PARITY_NONE); // default values
  uart_set_translate_crlf(MODEM, false); // default value
  uart_set_fifo_enabled(MODEM, true); // default value
  // MODEM Pico physical Pins 11 (TX), 12 (RX)
  gpio_set_function(UART_MODEM_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_MODEM_RX_PIN, GPIO_FUNC_UART);
  sleep_ms(100); //let the MODEM uart1 get settled
  // probably not needed for most KISS modems
  uart_putc(MODEM, '\r'); // wake up KISS
  sleep_ms(250);
  uart_putc(MODEM, '\r');
  sleep_ms(250);
  //uart_puts(MODEM, "Hello World\n"); // ok, that works
  
  // TEST
  //rtc_get_datetime(&t);
  //setFreq(std::string("ALERT HI"));  // now doen by GPIO from modem side
  // run loop once to test, and comment while(1) below
  //loop();
  //printf("crc match %X %X\n", 0x62, (uint8_t)std::stoul(std::string("0x62"), nullptr, 0));
  // queue initialization seems to work with #include "pico/util/queue.h"
  // queue_t *coreq;
  // queue_init(coreq, 32, 24);
  // enable multicore and move the GPS data crunching to core1
  // END TEST
  printf("Set Dummy RTC Date-Time: %4d-%02d-%02d %02d:%02d:%02d\n", t.year, t.month, t.day, t.hour, t.min, t.sec);
  //sleep_ms(1000);
  // NOTE: the device must be put into NMEA 0183 mode by the user first
  printf("\n\nMove the GPS handler to core1\n\n");
  b_rtcUpdated = false;
  multicore_launch_core1(handleGPSData);
  sleep_ms(2500); // let core1 get settled
  // merrily we roll along  
  while(true) {loop();}
}
