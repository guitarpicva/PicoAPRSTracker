#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/rtc.h"
#include "hardware/flash.h" // so we can read and write settings to flash at SETTINGS
#include "hardware/sync.h" // so we can disable and enable interrupts when flashing
#include "pico/multicore.h"
#include <string>
#include <vector>
#include "ez_cpp_utils.h"
#include "uikissutils.h"

#define MODEM uart1 
#define GPS uart0
  
#define UART_MODEM_TX_PIN 8u
#define UART_MODEM_RX_PIN 9u
#define MODEM_BAUD 4800 // for Nino Modem, fixed at 57600. Coastal Chipworks TNC-X set to 4800/9600 depending on jumpers

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
bool b_usbConnected = false; // avoid stdio if no USB connected
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
std::string incmd; // the stdin input string buffer

const std::string dest_addr = "APPCO1-0"; // dest SSID is always 0 in APRS
std::string source_addr = "AB4MW-12"; // for tracker boxes use SSID 12
std::string digi1 = "WIDE1-1"; // default APRS first digi
std::string digi2 = "WIDE2-2"; // default APRS second digi (I use 2-2 in my rural area)
std::string comment = "PicoAPRSTracker"; // the comment from the settings <= 36 chars
int i_interval = 3; // start with three (3) minute interval and adjust based on gps speed (eventually)
char c_objchar = '>'; // the object type for the APRS string defaults  to a Car

// write to flash at offset 0x3000000 to allow for storage.
// this is an OFFSET (from XIP_BASE) used by flash operations functions
// so the beginning address of the settings area is then XIP_BASE + SETTINGS
//#define SETTINGS_OFFSET XIP_NOCACHE_NOALLOC_BASE-XIP_BASE
//#define SETTINGS_OFFSET (256 * 1024)
// use the final sector in FLASH memory, well away from program code
#define SETTINGS_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

// pointer to char memory address of configuration data in FLASH memory
uint8_t *settings = (uint8_t *)(XIP_BASE + SETTINGS_OFFSET); 

// Flash write the new configuration to the FLASH memory.  This
// action should be followed by a read_config() in a running controller.
void flash_config(std::string cfg) { // a "|" delimited set of config params
  if(b_usbConnected) {printf("FlashLine:%s %d\n", cfg.c_str(), cfg.length());}
  uint pagelen = (cfg.length() + 1) / FLASH_PAGE_SIZE;
  if(((cfg.length() + 1) % FLASH_PAGE_SIZE) > 0) // only full pages are programmed
    pagelen++; // add 1 for the partial final page
  pagelen = pagelen * FLASH_PAGE_SIZE; // number of bytes for the flash_range_program call's length
  if(b_usbConnected) {printf("Write Buffer Size: %d bytes\n", pagelen);}
  uint8_t apage[pagelen]; // total bytes of our number of pages to flash
  for(int i = 0; i < cfg.length(); i++) {  // load the actual config bytes only
    apage[i] = (uint8_t)cfg.at(i);
    printf("%c", apage[i]);
  }
  apage[cfg.length()] = 0x00; // c_str terminator for the end of the data marker
  for(int i = cfg.length() + 1; i < pagelen; i++) {
    apage[i] = 'Z'; // fill the trailing bytes with z's
  }
  // calculate the number of sectors to erase to hold the pages to write
  uint sectorlen = pagelen / FLASH_SECTOR_SIZE;
  if((pagelen % FLASH_SECTOR_SIZE) > 0)
    sectorlen++;  // add a page for a trailer
  //printf("\nSector Count: %d", sectorlen);
  // stop interrupts and save thier state
  uint32_t ints = save_and_disable_interrupts();
  //printf("Interrupts Stored");
  flash_range_erase (SETTINGS_OFFSET, sectorlen); // erase 4096 byte sectors
  //printf("Settings Sectors Erased");
  flash_range_program(SETTINGS_OFFSET, apage, pagelen); //cfg.length() + 1);
  //printf("Settings Written");
  restore_interrupts (ints);
  //printf("Interrupts Restored");
  if(b_usbConnected) {printf("Finished FLASH write...\n");}
}

void read_config() {
  // settings is a pointer to the start of the flash where we
  // stored out settings data
  std::string flash_line((char *)settings);
  
  if(b_usbConnected) {
    //("\n\nRead Config Address: %02x\n", settings);
    // for(int i = 0; i < 256; i++)
    // {
    //   char inchar = (char) *(settings + i);
    //   if(inchar == 0x00) {
    //     //printf("BREAK!");
    //     break;}
    //   flash_line.push_back(inchar);
    // }
    //uint off = 0;
    //char inchar = (char) *settings;
    // while(inchar != 0x00) { // using a null to cap the end of the string, like c_str()
    //   //printf("[%d]:%c\n", off, inchar);
    //   off++;
    //   flash_line.append(1, inchar);
    //   inchar = (char) *(settings + off);
    // }
    printf("%s\n", flash_line.c_str());
  }
  // now our string holds the whole set of config params as string tokens
  // delimited by '|'

  // tokens are 0:source,1:digi1,2:digi2,3:comment,4:interval,5:object char
  std::vector<std::string> tokens; // six tokens only
  split(flash_line, tokens, '|', true);
  if(tokens.size() > 5) { // allows for more stuff on the end but at least 6 tokens
    source_addr = tokens[0];
    digi1 = tokens[1];
    digi2 = tokens[2];
    comment = tokens[3];
    i_interval = atoi(tokens[4].c_str());
    c_objchar = tokens[5].at(0);
  }
}

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
      split(gprmcsentence, vic, ',', true);
      //  for(int i = 0; i < vic.size(); i++) {
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
      //   0     1    2    3    4    5     6    7    8      9     10  11 12
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
        p.lon = vic[5].substr(0, 8); // for APRS format
        p.londir = vic[6];
        p.speed = vic[7]; // float knots
        p.course = vic[8]; // float degrees
        //printf("\rGPS Fix:%s Lat:%s%s Lon:%s%s Course:%s Speed:%s", p.fix.c_str(),p.lat.c_str(),p.latdir.c_str(),p.lon.c_str(),p.londir.c_str(),p.course.c_str(),p.speed.c_str());
        // update rtc if necessary
        if(p.fix == "A" && (b_rtcUpdated == false)) { // we have a fix and the clock is stale
          // update Pico RTC clock
          b_rtcUpdated = rtc_set_datetime(&t);
          //printf("RTCDT addr: %x", rtcdt);
          if(b_rtcUpdated) {
            // printf("Write RTC I2C: %d\n", i2c_write_blocking(i2c_default, GCA_ADDR, i2csend, 8, false));
            // sleep_ms(5); // ??
            if(b_usbConnected) printf("\nUpdated RTC: %4d-%02d-%02d %02d:%02d:%02d %d\n", t.year, t.month, t.day, t.hour, t.min, t.sec, b_rtcUpdated);
          }
        }
        // update the RTC at 59 past each hour
        if(t.min == 59) {
          if(!b_59Updated) {
            b_rtcUpdated = rtc_set_datetime(&t);
            if(b_usbConnected) printf("\nUpdated RTC: %4d-%02d-%02d %02d:%02d:%02d %d\n", t.year, t.month, t.day, t.hour, t.min, t.sec, b_rtcUpdated);
            b_59Updated = true;
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
        std::string mypos = "/"; // time/pos report
        if(t.day < 10) mypos += "0";
        mypos += std::to_string((int)t.day);
        if(t.hour < 10) mypos += "0";
        mypos += std::to_string((int)t.hour);
        if(t.min < 10) mypos += "0";
        mypos += std::to_string((int)t.min);
        mypos += 'z';
        mypos += p.lat;
        mypos += p.latdir;
        mypos += '/';
        mypos += p.lon;
        mypos += p.londir;
        mypos += c_objchar; // user definable in settings
        std::string val; // temp var
        if(p.course.length() == 0) {
          mypos += "000";
          //printf("course empty!");
        }
        else {
          val = p.course.substr(0, p.course.find('.'));
          //printf("course val:%s %d\n", val.c_str(), val.length());
          int times = (3 - val.length())          ;
          val.insert(0, times, '0');
          mypos += val;
        }
        mypos += '/';
        val.clear();
        if(p.speed.length() == 0) {
          mypos += "000";
          //printf("speed empty!");
        }
        else {
          val = p.speed.substr(0, p.speed.find('.'));
          //printf("speed val:%s %d %d\n", val.c_str(), val.length(), (3 - val.length()));
          int times = (3 - val.length())          ;
          val.insert(0,  times, '0');
          mypos += val;
        }
        mypos += comment.substr(0, 36); // comment for now but use settings value for comment
        if(b_usbConnected) printf("Sending:%s\n\n", mypos.c_str());
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
  // Loop through checking the MODEM serial first
  b_fullcmd = false;
  char inbyte='x';
  while(uart_is_readable(MODEM)) {
    //printf("READ MODEM");
    // read the byte, store it and look out for "\r" (0x0D)
    inbyte = uart_getc(MODEM);
    //printf("%02x ", inbyte); // ok that works
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
      // printf("\nKISS len: %d\n", kiss.length());
      // for(int i = 1; i < kiss.length(); i++) {
      //   printf("%02x ", kiss.at(i));
      // }
      if(kiss.length() > 0) {
         std::vector<std::string> partz = UIKISSUtils::unwrapUIFrame(kiss);
      //   printf("\nDest   :%s\n", partz[0].c_str());
      //   printf("Source :%s\n", partz[1].c_str());
      //   printf("Digi1  :%s\n", partz[2].c_str());
      //   printf("Digi2  :%s\n", partz[3].c_str());
      //   printf("Digi3  :%s\n", partz[4].c_str());
      //   printf("Payload:%s\n", partz[5].c_str());
      // }
      printf("%s>%s,%s,%s,%s\n%s\n\n", partz[1].c_str(), partz[0].c_str(), partz[2].c_str(), partz[3].c_str(), partz[4].c_str(), partz[5].c_str());
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

  // StdIn processing for configuration commands 
  // start by seeing if any user sdtin input is present
  b_fullcmd = false; // reused from above
  inbyte = getchar_timeout_us(50000); // inbyte reused below
  while(inbyte != 0xff) {
    incmd.append(1, inbyte);              
    if(inbyte == 0x0d) {      
      //printf("iincmd: %s\n", incmd.c_str());
      b_fullcmd = true;
      break; // only one line at a time

    }
    inbyte = getchar_timeout_us(100000);      
  }
  if(b_fullcmd) {
    if((incmd.length() > 0)) { //} && (incmd.find('\r') != std::string::npos)) {
      //printf("\nincmd: %s\n", incmd.c_str());
      std::vector<std::string> parts;
      split(incmd, parts, '|', true);
      // get rid of the carriage return on the end
      const std::string cmd = parts.at(0); //.substr(0, parts.at(0).length() - 1);
      //printf("StdIn cmd: %s\n", cmd.c_str());
      if(cmd == std::string("READCONFIG")) {
        //printf("Read the Config Data from FLASH\n");
        read_config();
      }
      else if(cmd == std::string("WRITECONFIG")) {
        // Trigger flash write here
        //printf("Trigger the Flash write of the config settings.\n");
        // may also need to do multicore_reset_core1() followed by
        //printf("Reset core1: %s", incmd.substr(12).c_str());
        multicore_reset_core1();
        flash_config(incmd.substr(12));
        // and finally relaunching multicore_launch_core1(handleGPSData);    
        sleep_ms(500);
        read_config();  
        sleep_ms(500);
        multicore_launch_core1(handleGPSData);
        //printf("restart core1");
      }
    }
    incmd.clear();
  }   
  b_usbConnected = stdio_usb_connected();
}

int main() {
  stdio_init_all();
  sleep_ms(1000); // give it a sec to catch up before printf's begin
  //flash_config(std::string("AB4MW-12|WIDE1-1|WIDE2-2|PicoAPRSTracker AB4MW|4|j"));
  //return 0;
  read_config();
  
  // startup stdio on the USB port for trace
  stdio_usb_init();
  //sleep_ms(500);  // 
  if(stdio_usb_connected()){b_usbConnected = true;}
  if(b_usbConnected) printf("USB stdio Connected...\n");
  
  // Start the RTC
  rtc_init();
  rtc_set_datetime(&t); // dummy DTG defined at the top!
  // Set up the GPS UART interface on 
  if(b_usbConnected) {
    printf("Set up GPS uart connection at %d baud\n", uart_init(GPS, GPS_BAUD));  //8N1 No flow control std. it seems  
    printf("GPS uart0 Enabled? %d\n", uart_is_enabled(GPS));  
  }
  // GPS module Pins 0, 1
  gpio_set_function(UART_GPS_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_GPS_RX_PIN, GPIO_FUNC_UART);

  // Now the MODEM uart1
  // Set up the MODEM UART interface on uart1
  if(b_usbConnected) {
    printf("Set up MODEM uart connection at %d and send KISS setup commands...\n", uart_init(MODEM, MODEM_BAUD));
    printf("MODEM uart1 Enabled? %d\n", uart_is_enabled(uart1));  
  }
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
  // uart_putc(MODEM, '\r'); // wake up KISS
  // sleep_ms(250);
  // uart_putc(MODEM, '\r');
    
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
  if(b_usbConnected) {
    printf("Set Dummy RTC Date-Time: %4d-%02d-%02d %02d:%02d:%02d\n", t.year, t.month, t.day, t.hour, t.min, t.sec);
    //sleep_ms(1000);
    // NOTE: the device must be put into NMEA 0183 mode by the user first
    printf("\n\nMove the GPS handler to core1\n\n");
  }
  b_rtcUpdated = false;
  multicore_launch_core1(handleGPSData);
  sleep_ms(2500); // let core1 get settled
  // TEST
  // END TEST
  // merrily we roll along  
  while(true) {loop();}
}
