/*
LED Adapter
 Date Created:  2014-11-19
 Using COM 6 on CB workstation
 
 TODO:
 - xxx consider a message queue that cycles so user can see every message
 - xxx continuously check ethernet_on flag, try to connect if false
 - xxx likewise, continuously check SD "enabled" flag
 - xxx consider replacing all String class uses with char arrays/pointers
 - xxx short ints
 - xxx triple-check memory allocation + deallocation
 */

/****************************************************************/
#include <LiquidCrystal.h>
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>

/****************************************************************/
//Create LCD screen instance ***
LiquidCrystal lcd(A5, A4, A3, A2, A1, A0); // (rs, enable, d4, d5, d6, d7) 

// Ethernet ***
byte mac[] = {
  0x90, 0xA2, 0xDA, 0x0F, 0x96, 0xBE};
byte ip[] = {
  10, 10, 151, 121};
unsigned short int server_port = 80;
EthernetServer server = EthernetServer(server_port);
EthernetClient client = EthernetClient();

// SD card ***
File                       f; // file to read from SD card

// diagnostic ***
unsigned short int         num_requests = 0;   // counter for requests
unsigned long              num_bytes_sent = 0; // counter for num bytes sent
unsigned long              num_bytes_rec = 0;  // counter for num bytes received
unsigned short int         status_code = 0;
String                     serial_debug_string = "";
unsigned long              time_of_last_serial_debug_print = millis();
unsigned short int         delay_ = 500;

// Parsing, Flags, State ***
byte                       cur_byte;
String                     token = "";
unsigned long              cur_line_num = 1;   // 1-based. 0 means no lines yet. (lines of incoming header)
unsigned short int         cur_token_index = 0;  // 0-based. e.g. GET will have token index 0. Filename 1. HTTP version 2.
String                     request_type = ""; // GET, POST
String                     file_path_name_args = ""; // file requested + args
String                     http_version_client = ""; // HTTP version specified by client
boolean                    appending_token = true;

/****************************************************************/
String get_response_header(String response_header_type, File f) // xxx   Make sure f is a pointer, to preserve memory.
{  
  // 'response_header_type' should be a media type, e.g. "image/png", "text/html".
  // http://en.wikipedia.org/wiki/Internet_media_type#Type_image
  String line_1 = "HTTP/1.1 200 OK\r\n";
  String line_2 = "Content-Type: " + response_header_type + "\r\n";
  String line_3 = "Content-Length: " + String(f.size()) + "\r\n";

  return line_1 + line_2 + line_3;
}

/****************************************************************/
void lcd_print(int x, int y, String msg)
{
  lcd.setCursor(x, y);
  lcd.print(msg);
}

/****************************************************************/
void lcd_print(int x, int y, IPAddress msg)
{
  lcd.setCursor(x, y);
  lcd.print(msg);
}

/****************************************************************/
// by David A. Mellis / Rob Faludi http://www.faludi.com
int availableMemory() {
  int size = 9999; // Use 2048 with ATmega328
  byte *buf;
  while ((buf = (byte *) malloc(--size)) == NULL)
    ;
  free(buf);
  return size;
}

/****************************************************************/
void setup()
{
  Serial.begin(9600); 

  // LCD screen
  lcd.begin(16,2);

  lcd_print(0, 0, "starting up...");

  // Ethernet
  while ( ! Ethernet.begin(mac) )
  {
    lcd.clear();
    lcd_print(0,0,"Ethernet failure");  
    delay(delay_);
  }
  server.begin(); // starts listening for incoming connections
  lcd.clear();
  lcd_print(0,0,"Ethernet on.");     
  delay(delay_);
  lcd.clear();
  lcd_print(0,0, Ethernet.localIP()); 
  delay(delay_);
  // SD card
  pinMode(10, OUTPUT);
  while ( ! SD.begin(4) )
  {
    lcd.clear();
    lcd_print(0,0,"SD failure.     "); 
    delay(delay_);
  }
  lcd.clear();
  lcd_print(0,0,"SD initialized. "); 
  delay(delay_);  
  lcd.clear();
}

/****************************************************************/
void loop()
{
  //  if (millis() - time_of_last_serial_debug_print > delay_)
  //  {
  //    Serial.print(serial_debug_string);
  //    time_of_last_serial_debug_print = millis();
  //    // clear serial message buffer
  //    serial_debug_string = "";
  //  }

  lcd_print(0, 0, String(availableMemory()));
  if (status_code < 10)
  {
    lcd_print(14, 1, " "); // clear error code 
  }
  lcd_print(13, 1, String(status_code)); // print error code
  lcd_print(0, 1, "q" + String(num_requests) );      // num requests
  lcd_print(4, 1, "s" + String(num_bytes_sent) );    // num bytes sent
  lcd_print(9, 1, "r" + String(num_bytes_rec));      // num bytes received

  if (client)
  {
    lcd_print(15, 1, "c");

    // read in all data from client, then process it.
    while (client.available())
      //cur_byte = ' ';
      //while(cur_byte != 0xFF)
    {
      cur_byte = client.read();

      // xxx  debugging
      //      server.write(cur_byte); 
      //      Serial.print(char(cur_byte));
      //      continue;

      if (char(cur_byte) == char(' '))
      {
        // space
        if (appending_token)
        {
          // current token ended
          appending_token = false;
          switch (cur_line_num)
          {
          case 1:
            switch(cur_token_index)
            {
            case 0:
              // request type (GET, POST, ...)
              request_type = token;
              Serial.print("1:0 token = " + token + "\n");
              break; 
            case 1:
              // file requested
              file_path_name_args = token;
              Serial.print("1:1 token = " + token + "\n");
              break; 
            case 2:
              // http version
              // This probably won't be reached because the last token should end with carriage return
              http_version_client = token;
              break; 
            default:
              // 400 (Bad Request)
              // xxx
              break; 
            }
            break;
          default:
            // xxx   if GET, ignore rest of lines but need to parse arguments from file path
            // xxx   if POST, need to get body
            break;
          }
        }
      }
      else if (char(cur_byte) == char('\r'))
      {
        Serial.print("  \\r\n");
        // carriage return
        if (appending_token)
        {
          // current token ended
          appending_token = false;
          switch (cur_line_num)
          {
          case 1:
            switch(cur_token_index)
            {
            case 2:
              http_version_client = token;
              Serial.print("1:2 token = " + token + "\n");
              break; 
            default:
              // 400 (Bad Request)
              Serial.print("1:");
              Serial.print(cur_token_index);
              Serial.print("\n");
              // xxx
              break; 
            }
          default:
            Serial.print(cur_line_num);
            Serial.print(":");
            Serial.print(cur_token_index);
            Serial.print("\n");
            break;
          }
        }
      }
      else if (char(cur_byte) == char('\n'))
      {
        // newline
        Serial.print("  \\n\n");
        cur_line_num ++;
        cur_token_index = 0;
      }
      else
      {
        // non-whitespace 
        if (!appending_token)
        {
          // starting a new token
          token = "";
          cur_token_index ++;
          // check length of token, make sure it's not too long
          // xxx
        }
        appending_token = true;
        token += String(char(cur_byte));
      }
    }// end while
    // send response

      // parse out arguments in file path
    ;

    // retrieve the requested file
    ;
    // send header
    Serial.print("sending response header\n");
    server.println( get_response_header("text/html", f ) );
    Serial.print("response header sent.\n");

    // send file

    // done sending response; discharge client
    if (client.connected())
    {
      client.stop(); 
      Serial.print("stopping client\n");
    }
    else
    {
      Serial.print("client already stopped !!!\n"); 
    }
  }// end if(client)
  else
  {
    // No client. 
    lcd_print(15, 1, " "); // clear 'c'

    // Check for incoming client.
    client = server.available(); 
  }
}


















