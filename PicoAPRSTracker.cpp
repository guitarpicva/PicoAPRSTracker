#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/rtc.h"
#include "pico/multicore.h"
#include <string>
#include <vector>
#include "ez_cpp_utils.h"
#include "uikissutils.h"

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

bool b_sendReady = false; // flag to trigger sending output data to modem in loop()
bool b_rtcUpdated = false; // if the clock has been updated at start (of course not!)
bool b_59Updated = false; // if the 59th minute RTC update has occurred
bool b_beaconSent = false; // if the beacon has been sent this minute already
bool b_fullcmd = false; // used by input queue handling
bool b_curr_chan = false; // LO channel by default
// Dummy start time for the RTC the struct is 8 bytes in length
datetime_t t = {
        .year  = 2022,
        .month = 10,
        .day   = 30,
        .dotw  = 0, // 0 is Sunday
        .hour  = 16,
        .min   = 9,
        .sec   = 0
};

// current position set to default of no position held (startup condition)
position_t p = {
    .fix = "V",
    .lat = "0000.00",
    .latdir = "N",
    .lon = "00000.00",
    .londir = "W",
    .speed = "000",
    .course = "000"
};

std::string outq;  // output byte queue to the MODEM UART
std::string reply; // input byte queue from the MODEM UART
std::string gpsdata; // the buffer for the function running on core1

const std::string dest_addr = "APPICO-0"; // SSID is always 0 in APRS
const std::string source_addr = "AB4MW-12"; // for tracker boxes use SSID 12
const std::string digi1 = "WIDE1-1";
const std::string digi2 = "WIDE2-2";
int i_interval = 2; // start with one (2) minute interval and adjust based on gps speed

void loadOutQueue(std::string toSend, bool truncate) {
  //printf("%s", toSend.data());
  // add bytes to the outq for transmission to the MODEM
  // next time thru the loop
  if(truncate) {
    outq.clear();
    //Serial.println("outq is empty!");
  }
  outq += toSend;
  b_sendReady = true;
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
  bool newData = false; // are we getting data from the GPS yet?
  bool b_59Updated = false; // marks 59 minute updated so only once
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
        
    if (newData) // if we got new data, then process it
    {
      //printf("%s\r", gpsdata.c_str());      // ok works
      size_t gprmc = gpsdata.find("$GPRMC"); // start of the gprmc sentence
      if(gprmc < 0) {gpsdata.clear(); b_rtcUpdated = false; continue;} // continue outer loop on missing GPRMC sentence
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
        t.min   = (int8_t)std::stoi(vic[1].substr(2, 2)); //printf("t.min:%d\r", (int)t.min);
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
        p.speed = vic[7]; // float
        p.course = vic[8]; // float
        //printf("\rGPS Fix:%s Lat:%s%s Lon:%s%s Speed:%s Course:%s", p.fix.c_str(),p.lat.c_str(),p.latdir.c_str(),p.lon.c_str(),p.londir.c_str(),p.speed.c_str(),p.course.c_str());
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
            b_rtcUpdated = rtc_set_datetime(&t);
            printf("\nUpdated RTC: %4d-%02d-%02d %02d:%02d:%02d %d\n", t.year, t.month, t.day, t.hour, t.min, t.sec, b_rtcUpdated);
          }
        }
        else {
          if(b_59Updated) b_59Updated = false; // reset it at minute zero or thereafter
        }
      } // otherwise no data or CRC bad so just keep looping
    }
    // beacon building based on min of hour
    // possible intervals are all even divisors of 60 up to 30 (2,3,4,5,6,10,12,15,20,30)
    // and the GPS position speed values determines how low the interval gets (inversely)
    if((t.min % (uint8_t)i_interval) == 0) {
      //printf("beacon minute:%d\n", t.min);
      if(!b_beaconSent) {// only send once per sending minute
        // /dayhourminzlatDir/lonDir>course/speedcomment
        char *tmp;
        std::string mypos = "/";
        if(t.day < 10) mypos += "0";
        mypos += std::to_string((int)t.day);
        if(t.hour < 10) mypos += "0";
        mypos += std::to_string((int)t.hour);
        if(t.min < 10) mypos += "0";
        mypos += std::to_string((int)t.min);
        mypos += "z";
        mypos += p.lat;
        mypos += p.latdir;
        mypos += "/";
        mypos += p.lon;
        mypos += p.londir;
        mypos += ">";
        if(p.course.empty())
          mypos += "000";
        else
          mypos += p.course;
          mypos += "/";
        if(p.speed.empty())
          mypos += p.speed;
        else
          mypos += "000";
        mypos += source_addr; // comment for now
        //printf("mypos:%s\n", mypos.c_str());
        // send the beacon
        loadOutQueue(UIKISSUtils::kissWrap(UIKISSUtils::buildUIFrame(dest_addr, source_addr, digi1, digi2, mypos)), true);
        b_beaconSent = true;
      }
    }
    else {
      // not a sending minute so reset the bool
      b_beaconSent = false;
    }
  } // end WHILE (true)
}

void loop() {
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // REMEMBER: the t and p variables should be treated as READ ONLY
  // from this core because they are constantly changing in the other core
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
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
  while(uart_is_readable(MODEM)) {
    //printf("READ MODEM");
    // read the byte, store it and look out for "\r" (0x0D)
    inbyte = uart_getc(MODEM);
    printf("\n%0x", inbyte); // ok that works
    if(reply.empty() && (inbyte == 0x00)) {continue;} // startup seems to send a NULL
    reply.append(1, inbyte);
    //printf("\nReply size:%02d", reply.length());
    // the below means we only take one KISS command at a time
    if((reply.length() > 2) && (inbyte == UIKISSUtils::FEND)) {
      //printf("\nFull Cmd\n");
      b_fullcmd = true;
      break; // change this to a function call to handle the full command
    }      
  }
  // post-process the input buffer from the MODEM
  if(b_fullcmd) { // should be a fully formed MODEM reply followed by \r\n
    // here we process the full command from the MODEM
    
    if(reply.length() > 2) {
      // for(int i = 0; i < reply.length(); i++) {
      //   printf("R:%02x ", reply.at(i));
      // }

      std::string kiss = UIKISSUtils::kissUnwrap(reply);
      //printf("\nKISS len: %d\n", kiss.length());
      for(int i = 1; i < kiss.length(); i++) {
        printf("%02x ", kiss.at(i));
      }
    }
    reply.clear();
  }
  
  // Now send commands to the MODEM if necessary
  if(b_sendReady && outq.length() > 0){
    //printf("\nKISS Cmd Out: %s\n", outq.c_str());
    // for(int i = 0; i < outq.length(); i++) {
    //   uart_putc_raw(MODEM, outq.at(i));
    //   //printf("%02x ", (uint8_t)outq.at(i));
    // }
    uart_write_blocking(MODEM, (uint8_t *)outq.c_str(), outq.length());
    //uart_write_blocking(MODEM, (uint8_t*)outq.data(), outq.length());       
    outq.clear(); // clear the outq for next loop
    b_sendReady = false;
  }    
}

int main() {
  stdio_init_all();
  sleep_ms(1000); // give it a sec to catch up before printf's begin
  // startup stdio on the USB port for trace
  stdio_usb_init();
  while(!stdio_usb_connected()){;}
  printf("USB Connected...\n");
  //outq = "HELLO";
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
