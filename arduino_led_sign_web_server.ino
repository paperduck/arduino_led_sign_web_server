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
File *                     f; // file to read from SD card

// diagnostic ***
unsigned short int         num_requests = 0;   // counter for requests
unsigned long              num_bytes_sent = 0; // counter for num bytes sent
unsigned long              num_bytes_rec = 0;  // counter for num bytes received
unsigned short int         status_code = 0;
String                     serial_debug_string = "";
unsigned long              time_of_last_serial_debug_print = millis();
unsigned short int         d1 = 100;
unsigned short int         d5 = 500;
int                        memory_available = 0;
int                        memory_available_min = 9999;

// Parsing, Flags, State ***
byte                       cur_byte;
String                     token = "";
unsigned long              cur_line_num = 1;   // 1-based. 0 means no lines yet. (lines of incoming header)
unsigned short int         cur_token_index = 0;  // 0-based. e.g. GET will have token index 0. Filename 1. HTTP version 2.
String                     request_type = ""; // GET, POST
String                     file_path_name = ""; // file requested + args
char *                     file_path_name_buf; // char* for passing into SD.exists(), SD.open()
//String                     file_path_name_trimmed = ""; // file requested + args
String                     http_version_client = ""; // HTTP version specified by client
boolean                    appending_token = true;
long                       index_of; // stores result of String.indexOf()


/****************************************************************/
String get_response_header(String response_header_type, File * f) // xxx   Make sure f is a pointer, to preserve memory.
{  
  // 'response_header_type' should be a media type, e.g. "image/png", "text/html".
  // http://en.wikipedia.org/wiki/Internet_media_type#Type_image
  String line_1 = String("HTTP/1.1 200 OK\r\n");
  String line_2 = String("Content-Type: ") + response_header_type + String("\r\n");
  String line_3 = String("Content-Length: ") + String(f->size()) + String("\r\n");

  return String(line_1) + String(line_2) + String(line_3);
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
  int size = 2048; // Use 2048 with ATmega328
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

  lcd_print(0, 0, ".");

  // Ethernet
  while ( ! Ethernet.begin(mac) )
  {
    lcd.clear();
    lcd_print(0,0,"E !!");  
    delay(d1);
  }
  server.begin(); // starts listening for incoming connections
  lcd.clear();
  lcd_print(0,0,"E  ok");     
  delay(d5);
  lcd.clear();
  lcd_print(0,0, Ethernet.localIP()); 
  delay(d5);

  // SD card
  f = (File *)malloc(sizeof(File));
  pinMode(10, OUTPUT);
  while ( ! SD.begin(4) )
  {
    lcd.clear();
    lcd_print(0,0,"SD !!"); 
    delay(d1);
  }
  lcd.clear();
  lcd_print(0,0,"SD ok"); 
  delay(d5);  
  lcd.clear();
}

/****************************************************************/
void loop()
{
  //  if (millis() - time_of_last_serial_debug_print > d1)
  //  {
  //    Serial.print(serial_debug_string);
  //    time_of_last_serial_debug_print = millis();
  //    // clear serial message buffer
  //    serial_debug_string = "";
  //  }

  memory_available = availableMemory();
  if (memory_available < memory_available_min)
  {
    memory_available_min = memory_available;
    lcd_print(0, 0, "    ");
  }
  lcd_print(0, 0, String( memory_available_min ));  
  //  if (status_code < 10)
  //  {
  //    lcd_print(14, 1, " "); // clear error code 
  //  }
  //  lcd_print(13, 1, String(status_code)); // print error code
  //  lcd_print(0, 1, "q" + String(num_requests) );      // num requests
  //  lcd_print(4, 1, "s" + String(num_bytes_sent) );    // num bytes sent
  //  lcd_print(9, 1, "r" + String(num_bytes_rec));      // num bytes received

  if (client)
  {
    lcd_print(15, 1, "c");
    appending_token = true;
    cur_line_num = 1;
    cur_token_index = 0;
    // read in all data from client, then process it.
    while (client.available())
    {
      cur_byte = client.read();

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
              break; 
            case 1:
              // file requested
              file_path_name = token;
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
              break; 
            default:
              // 400 (Bad Request)
              // xxx
              break; 
            }
          default:
            break;
          }
        }
      }
      else if (char(cur_byte) == char('\n'))
      {
        // newline
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
    index_of = file_path_name.indexOf("?", 0);
    if (index_of > -1)
    {
      // extract arguments  xxx
      file_path_name = file_path_name.substring(0, index_of); 
    }

    // retrieve the requested file
    //    file_path_name_buf = (char*)malloc( (sizeof(char) * file_path_name.length()) + 1 );
    //    file_path_name_buf = (char*)malloc( 11 );
    //    file_path_name.toCharArray(file_path_name_buf, (file_path_name.length() + 1));
    //    file_path_name.toCharArray(file_path_name_buf, 11);
    file_path_name_buf = "/index.htmx";
    file_path_name_buf[10] = '\n';
    Serial.print( file_path_name_buf );
    if (SD.exists(file_path_name_buf))
    {
      lcd_print(4, 0, "s+");
      *f = SD.open(file_path_name_buf, FILE_READ);
      if (f->available())
      {
        lcd_print(6, 0, "f+");
        Serial.print( "1\n" ); // costs 17 bytes?
      }
      else
      {
        lcd_print(6, 0, "f-");
        Serial.print( "2\n" ); // costs 17 bytes?
        // xxx  204 No Content 
      }
    }
    else
    {
      lcd_print(4, 0, "s-");
      // xxx  404 Not Found
    }
    free(file_path_name_buf); // super-important memory deallocation
    // send header
    // server.println( get_response_header("text/html", f ) );

    // send file
    while (f->available())
    {
      server.write(f->read());
    }

    // done sending response; discharge client
    if (client.connected())
    {
      client.stop(); 
      ;
    }
    else
    {
      ;
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


























