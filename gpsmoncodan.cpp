#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/rtc.h"
#include "hardware/i2c.h" // slave the RTC datetime object
#include "pico/multicore.h"
#include <string>
#include <vector>

#define RADIO uart1
#define GPS uart0
  
#define UART_RADIO_TX_PIN 8u
#define UART_RADIO_RX_PIN 9u
#define UART_GPS_TX_PIN 0u
#define UART_GPS_RX_PIN 1u
#define RADIO_BAUD 9600
#define GPS_BAUD 9600
#define GCA_ADDR 0u  // the i2c broadcast address
// Pico pin 4 as GPIO pin 2 to receive a trigger to change channel
// via GPIO state: high is HI channel and low is LO channel
#define SET_CHANNEL 2u

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

//const char CR = 0x0D;
//const std::string TERM = "\r\n"; // the end of a CODAN command response
//const std::string s_CR = "\r"; // a carriage return only for scan purposes
std::string SET_ECHO_OFF = "echo off\r";
std::string SET_PROMPT_OFF = "prompt off\r";
std::string outq;  // output byte queue to the radio UART
std::string reply; // input byte queue from the radio UART
std::string gpsdata; // the buffer for the function running on core1

void loadOutQueue(std::string toSend, bool truncate) {
  //printf("%s", toSend.data());
  // add bytes to the outq for transmission to the RADIO
  // next time thru the loop
  if(truncate) {
    outq.clear();
    //Serial.println("outq is empty!");
  }
  outq += toSend;
}

void setFreq(std::string s_chan) { 
  // channel name always in a quoted string for CODAN followed by carriage return
  // channel change is always priority so truncate = true
  loadOutQueue("CHAN \'" + s_chan + "\'\r", true);
}

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
}

void handleGPSData() {
  gpsdata.clear();
  b_rtcUpdated = false; // initialize so gets set first time thru
  bool newData = false;
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
  
    // for (start; (to_ms_since_boot(get_absolute_time()) - start) < 1000;)
    // {
    while ((to_ms_since_boot(get_absolute_time()) - start) < 1000)
    {
      if(uart_is_readable(GPS)) {
        //printf("READ GPS");
        gpsdata.push_back(uart_getc(GPS));
        if(!newData) newData = true;  
      }
    } // end read
    if (newData)
    {
      //printf("%s\n", gpsdata.c_str());      // ok works
      size_t gprmc = gpsdata.find("$GPRMC"); // start of the gprmc sentence
      if(gprmc < 0) {gpsdata.clear(); b_rtcUpdated = false; continue;} // continue on missing sentence
      size_t endex = gpsdata.find("\r\n", gprmc); // end of the gprmc sentence
      
      std::string gprmcsentence = gpsdata.substr(gprmc, endex - gprmc);
      //printf("%s\n", gprmcsentence.c_str());
      std::vector<std::string> vic;
      split(gprmcsentence, vic, ',');
      // for(int i = 0; i < vic.size(); i++) {
      //   printf("%d: %s\n", i, (vic[i]).c_str());
      // }
      // printf("\n");
      gpsdata.clear(); // for the next iteration
      if(vic[2].compare(std::string("A")) != 0) {b_rtcUpdated = false; continue;}  // no fix yet      
      if(gprmcsentence.length() < 25) {b_rtcUpdated = false; continue;} // no fix yet so just loop (prob not needed)  
      size_t star = gprmcsentence.length() - 3; // work back from start of end 3 positions 
      //std::string s_crc("0x");
      std::string s_crc = gprmcsentence.substr(star+1, 2);
      //printf("s_crc: %s\n", s_crc.c_str());
      uint8_t g_crc = (uint8_t)std::stoi(s_crc, nullptr, 16);
      uint8_t crc = 0;
      for(int i = 1; i < star; i++)
      {
        // this adds the crc
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
        printf("GPS : %4d-%02d-%02d %02d:%02d:%02d\r",t.year, t.month, t.day, t.hour, t.min, t.sec);        
        // decide when to update the RTC
        //printf("t.min: %02d\n", t.min);
        if(t.min == 59) {
          if(!b_59Updated) {
            b_rtcUpdated = rtc_set_datetime(&t);
            //sleep_ms(2);
            uint8_t i2csend[8]; 
            uint8_t *rtcdt = (uint8_t *)&t;
            //printf("\n\nRTC DT Buffer: ");
            i2csend[0] = (uint8_t)(t.year >> 8); // hi byte
            i2csend[1] = (uint8_t)(t.year & 0x00ff); // lo byte
            printf("\n\n%02d ", i2csend[0]);          
            printf("%02d \n\n", i2csend[1]);          
            for(int i = 2; i < 8; i++) {
              i2csend[i] = rtcdt[i];
            }
            // last minute of the hour so force an update to the RTC
            if(b_rtcUpdated) {
              i2c_write_blocking(i2c_default, GCA_ADDR, i2csend, 8, false);            
              b_59Updated = true;
              sleep_ms(5); // ??
              printf("\n\nUpdated RTC 59: %4d-%02d-%02d %02d:%02d:%02d %d\n\n", t.year, t.month, t.day, t.hour, t.min, t.sec, b_rtcUpdated);
            }
          }
        }
        else if(b_rtcUpdated == false) {
          // rtc has yet to be updated or has been invalidated, so update            
          b_rtcUpdated = rtc_set_datetime(&t);
          // TEST
          //sleep_ms(2);
          uint8_t i2csend[8]; 
          uint8_t *rtcdt = (uint8_t *)&t;
          //printf("\n\nRTC DT Buffer: ");
          i2csend[0] = (uint8_t)(t.year >> 8);
          i2csend[1] = (uint8_t)(t.year & 0x00ff);
          //printf("%02d ", i2csend[0]);          
          //printf("%02d ", i2csend[1]);          
          for(int i = 2; i < 8; i++) {
            i2csend[i] = rtcdt[i];
            //printf("%02d ", i2csend[i]);          
          }
          //printf("\n");
          // END TEST
          //printf("RTCDT addr: %x", rtcdt);
          if(b_rtcUpdated) {
            printf("Write RTC I2C: %d\n", i2c_write_blocking(i2c_default, GCA_ADDR, i2csend, 8, false));
            sleep_ms(5); // ??
            printf("\nUpdated RTC: %4d-%02d-%02d %02d:%02d:%02d %d\n\n", t.year, t.month, t.day, t.hour, t.min, t.sec, b_rtcUpdated);
          }
        }
        else {
          if(b_59Updated) b_59Updated = false; // reset it at minute zero or thereafter
        }
      }
    }
  } // end WHILE
}

void loop() {
  //printf("Looping...\r");
  // Check GPIO 2 to possibly change channel
  if(gpio_get(SET_CHANNEL) != b_curr_chan) { 
    b_curr_chan = gpio_get(SET_CHANNEL);
    if(b_curr_chan)
      setFreq(std::string("ALERT HI"));  
    else
      setFreq(std::string("ALERT LO"));  
    printf("\nSet ALERT channel... %s", gpio_get(SET_CHANNEL)?"HI":"LO");    
  }
  // Loop through checking the RADIO serial first
  // For HFA we only care about the minute of hour value from the GPS
  b_fullcmd = false;
  char inbyte='x';
  reply.clear();
  while(uart_is_readable(RADIO)) {
    //printf("READ RADIO");
    // read the byte, store it and look out for "\r" (0x0D)
    inbyte = uart_getc(RADIO);
    //printf((char *)&inbyte);
    reply.push_back(inbyte);
    if(inbyte == 0x0D) {
      inbyte = uart_getc(RADIO);  // we don't need the trailing line feed so just get it out of the way
      b_fullcmd = true;
      break; // change this to a function call to handle the full command
    }      
  }
  // post-process the input buffer from the RADIO
  if(b_fullcmd) { // should be a fully formed CODAN reply followed by \r\n (usually, otherwise only \r)
    // here we process the full command from the RADIO
    if(reply.length() > 2)
      printf("CICS Reply: %s\n", reply.c_str());
    //printf(reply.data());
  }
  
  // Now send commands to the radio if necessary
  if(outq.length() > 0){
    printf("\nCICS Command: %s\n", outq.c_str());
    for(int i = 0; i < outq.length(); i++) {
      uart_putc_raw(RADIO, outq.at(i));
      //printf("%02x ", (uint8_t)outq.at(i));
    }
    //uart_write_blocking(RADIO, (uint8_t*)outq.data(), outq.length());       
    outq.clear(); // clear the outq for next loop
  }    
}

int main() {
  stdio_init_all();
  sleep_ms(1000); // give it a sec to catch up before printf's begin
  // set up i2c broadcast as master (the default)
  i2c_init(i2c_default, 100000);  // Master at 100 khz rate (max), we are only sending 8 bytes
  gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
  gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
  
  // Start the RTC
  rtc_init();
  rtc_set_datetime(&t); // dummy DTG defined at the top!
  // Set up the GPS UART interface on uart0
  printf("Set up GPS uart connection at %d\n", uart_init(GPS, GPS_BAUD));  //8N1 No flow control std. it seems  
  printf("GPS uart0 Enabled? %d\n", uart_is_enabled(GPS));  
  // GPS module Pins 0, 1
  gpio_set_function(UART_GPS_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_GPS_RX_PIN, GPIO_FUNC_UART);

  // pin for triggering channel changes
  gpio_init(SET_CHANNEL);
  gpio_set_dir(SET_CHANNEL, GPIO_IN);

  // Now the RADIO uart1
  // Set up the Radio UART interface on uart1
  printf("Set up radio uart connection at %d and send CICS setup commands...\n", uart_init(RADIO, RADIO_BAUD));
  printf("RADIO uart1 Enabled? %d\n", uart_is_enabled(uart1));  
  // turn off RTS/CTS flow control
  uart_set_hw_flow(RADIO, false, false); // defaults not specified 
  //uart_set_format(RADIO, 8, 1, UART_PARITY_NONE); // default values
  uart_set_translate_crlf(RADIO, false); // default value
  uart_set_fifo_enabled(RADIO, true); // default value
  // Radio Pins 11, 12
  gpio_set_function(UART_RADIO_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_RADIO_RX_PIN, GPIO_FUNC_UART);
  sleep_ms(100); //let the RADIO uart1 get settled
  uart_putc(RADIO, '\r'); // wake up CICS
  sleep_ms(250);
  uart_putc(RADIO, '\r');
  sleep_ms(250);

  //uart_write_blocking(RADIO, (uint8_t *)SET_ECHO_OFF.data(), 9);
  uart_write_blocking(RADIO, (uint8_t *)"ECHO OFF\r", 9);
  sleep_ms(250);
  //uart_write_blocking(RADIO, (uint8_t*) SET_PROMPT_OFF.data(), 11);
  uart_write_blocking(RADIO, (uint8_t*)"PROMPT OFF\r", 11);
  sleep_ms(1000);

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
