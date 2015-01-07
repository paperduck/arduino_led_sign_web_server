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
 - xxx retry upon 404, 204 a certain number of times
 */

/*
Notes:
 
 Serial.print( "2\n" ); // costs 17 bytes?
 file_path_name_bu (char*)malloc( 11 ); // reserves extra 2 bytes of Arduino memory (e.g. malloc(12) -> 14 bytes used)
 file_path_name_buf = "/index.htm"; // calling this repeatedly than once doesn't consume more memory
 */

/****************************************************************/
//#include <LiquidCrystal.h>
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

/****************************************************************/
//Create LCD screen instance -----------------------------------
//LiquidCrystal lcd(A5, A4, A3, A2, A1, A0); // (rs, enable, d4, d5, d6, d7) 

// Ethernet -----------------------------------
byte mac[] = {
  0x90, 0xA2, 0xDA, 0x0F, 0x96, 0xBE};
//byte ip[] = {
//  10, 10, 151, 121};
EthernetServer server = EthernetServer(80);
EthernetClient client = EthernetClient();

// SD card -----------------------------------
//File                       f; // file to read from SD card

// Serial -----------------------------------
SoftwareSerial             s_sign(2,3);
SoftwareSerial             s_pc(5,6);   // pin 4 used for ethernet shield's SD card slave select 
//byte                       b;
boolean                    serial_buffer_empty = true;
long                       time_since_last_rec_byte = millis() + 60000;
const unsigned short int   chunk_size = 3;
byte                       sign_packet_buffer[chunk_size]; 
unsigned short int         num_bytes_sent_serial = 0; // counter for num bytes sent
unsigned short int         num_bytes_rec_serial = 0;  // counter for num bytes received

// diagnostic -----------------------------------
//unsigned short int         num_requests = 0;   // counter for requests
unsigned short int         status_code = 0;
String                     serial_debug_string = "";
unsigned long              time_of_last_serial_debug_print = millis();
unsigned short             d5 = 500;
//short                      memory_available = 0;
//short                      memory_available_min = 9999;

// Parsing, Flags, State -----------------------------------
byte                       cur_byte;
String                     token = "";
unsigned long              cur_line_num = 1;   // 1-based. 0 means no lines yet. (lines of incoming header)
unsigned short int         cur_token_index = 0;  // 0-based. e.g. GET will have token index 0. Filename 1. HTTP version 2.
String                     request_type = ""; // GET, POST
String                     file_path_name = ""; // file requested + args
//char *                     file_path_name_buf; // char* for passing into SD.exists(), SD.open()
//String                   file_path_name_trimmed = ""; // file requested + args
//String                   http_version_client = ""; // HTTP version specified by client
boolean                    appending_token = true;
long                       index_of; // stores result of String.indexOf()

// File stuff -----------------------------------
String                     file_media_type; // a.k.a. MIME type, a.k.a. Content-type
String                     default_media_type;

// debugging -----------------------------------
byte                       temp_byte;

// EEPROM - 1024 bytes -----------------------------------
unsigned short int         sign_buffer_start = 0;
unsigned short int         eeprom_write_counter = 0;

/****************************************************************/
void setup()
{
  Serial.begin(9600); 
  s_sign.begin(2400); // !!! the order that these serials are begun seems to matter !!!
  s_pc.begin(2400); // !!! the order that these serials are begun seems to matter !!!

  // LCD screen
  //  lcd.begin(16,2);

  //Serial.println(F("."));

  // Ethernet
  while ( ! Ethernet.begin(mac) )
  {
    //Serial.println(F("eth"));
    delay(10);
  }
  Serial.println(F("eth OK"));
  server.begin(); // starts listening for incoming connections

  // SD card
  //f = (File *)malloc(sizeof(File));
  pinMode(10, OUTPUT);
  while ( ! SD.begin(4))
  {
    //Serial.println( F("SD") );
    delay(10);
  }
  Serial.println(F("SD OK"));

  default_media_type = F("application/octet-stream");
} 

/****************************************************************/
// multiline uses same SRAM as inline concatenation
// 'file_media_type' should be a media type, e.g. "image/png", "text/html".
//   http://en.wikipedia.org/wiki/Internet_media_type#Type_image
//
boolean send_http_header(unsigned int file_size, String file_media_type, unsigned short status_code)
{  
  server.print( "HTTP/1.1 200 OK\r\nContent-Type: " );
  server.print( file_media_type);
  server.print( "\r\nContent-length: " );
  server.print( String(file_size) );
  server.print( "\r\n\r\n" );
}

/****************************************************************/
//// by David A. Mellis / Rob Faludi http://www.faludi.com
//int availableMemory() {
//  int size = 2048; // Use 2048 with ATmega328
//  byte *buf;
//  while ((buf = (byte *) malloc(--size)) == NULL)
//    ;
//  free(buf);
//  return size;
//}

/****************************************************************/
//void refresh_memory_stats()
//{
//  memory_available = availableMemory();
//  if (memory_available < memory_available_min)
//  {
//    memory_available_min = memory_available;
//    lcd_print(0, 0, "    ");
//    lcd_print(0, 1, "    ");
//  }
//  lcd_print(0, 0, String( memory_available )); 
//  lcd_print(0, 1, String( memory_available_min )); 
//}

/****************************************************************/
// Returns file media type

String get_file_media_type(String file_name)
{
  long index_of = file_name.lastIndexOf(".");

  if (index_of == -1)
  {
    // no period found, use a default type
    return default_media_type;
  }
  else if (file_name.substring(index_of) == F(".htm") || file_name.substring(index_of) == F(".html"))
  {
    return F("text/html");
  }
  else if (file_name.substring(index_of) == F(".png"))
  {
    return F("image/png");
  }
  else
  {
    // default case
    return default_media_type; 
  }
}

/****************************************************************/
void process_client()
{

  //  lcd_print(15, 1, "c");
  appending_token = true;
  cur_line_num = 1;
  cur_token_index = 0;
  // read in all data from client, then process it.
  while (client.available())
  {
    cur_byte = client.read();
    //Serial.write(cur_byte);

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
            file_path_name.toLowerCase(); // convert to lower case 
            if (file_path_name == "/" || file_path_name == "\\")
            {
              file_path_name = "index.htm";
            }
            break; 
          case 2:
            // http version
            // This probably won't be reached because the last token should end with carriage return
            //http_version_client = token;
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
            //http_version_client = token;
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

    // parse out arguments in file path
  index_of = file_path_name.indexOf("?", 0);
  if (index_of > -1)
  {
    // extract arguments  xxx
    file_path_name = file_path_name.substring(0, index_of); 
  }

  // retrieve the requested file
  char * file_path_name_buf = (char*)malloc( (sizeof(char) * file_path_name.length()) + 1 );
  file_path_name.toCharArray(file_path_name_buf, (file_path_name.length() + 1));

  if (SD.exists(file_path_name_buf)) // costs 119 bytes ?
  {
    //    lcd_print(4, 0, "+");  
    File f = SD.open(file_path_name_buf, FILE_READ);  // costs 31 bytes ?     
    if (f.available())
    {
      //      lcd_print(6, 0, "+");

      // xxx determine content-type based on filename
      // send header
      send_http_header(f.size(), get_file_media_type(file_path_name), 200);
      // send file
      while (f.available())
      {
        temp_byte = f.read();
        server.write(temp_byte);
        //Serial.write(temp_byte); // debugging        
      }
    }
    else
    {
      // xxx  204 No Content 
      //      lcd_print(6, 0, "-");
      send_http_header(3,  F("text/plain"), 204 );
      server.print(F("204")); // no content
    }
    // close file 
    f.close();
  }
  else
  {
    // xxx  404 Not Found
    //    lcd_print(4, 0, "-");
    send_http_header(3,  F("text/plain"), 404 );
    server.print(F("404")); // not found
  }
  //   disconnect client, if there is no undread data from client.
  if (client.connected())
  {
    client.stop(); 
  }
  free(file_path_name_buf); // super-important memory deallocation
  token = "";
  request_type = "";
  file_path_name = "";
  // xxx any others?
}

/****************************************************************/
void loop()
{
  //  if (millis() - time_of_last_serial_debug_print > 10)
  //  {
  //    Serial.print(serial_debug_string);
  //    time_of_last_serial_debug_print = millis();
  //    // clear serial message buffer
  //    serial_debug_string = "";
  //  }

  //refresh_memory_stats();

  //  if (status_code < 10)
  //  {
  //    lcd_print(14, 1, " "); // clear error code 
  //  }
  //  lcd_print(13, 1, String(status_code)); // print error code
  //  lcd_print(0, 1, "q" + String(num_requests) );      // num requests
  //  lcd_print(4, 1, "s" + String(num_bytes_sent) );    // num bytes sent
  //  lcd_print(9, 1, "r" + String(num_bytes_rec));      // num bytes received


  //  if (client)
  //  {
  //    process_client();
  //  }// end if(client)
  //  else
  //  {
  //    // No client. 
  //    //    lcd_print(15, 1, " "); // clear 'c'
  //
  //    // Check for incoming client.
  //    client = server.available(); 
  //  }



  // listen for incoming serial communication
  // xxx make sure bytes aren't arriving slower than i process them - consider delaying to allow EEPROM to finish filling
  if (s_pc.available())
  {    
    // read byte into EEPROM
    if (eeprom_write_counter < 80)
    {
      EEPROM.write(num_bytes_rec_serial, s_pc.read() );
      eeprom_write_counter++;
    }
    else
    {
      return;
    }
    num_bytes_rec_serial ++;
    serial_buffer_empty = false;
    
    Serial.print("rec: ");
    Serial.print(num_bytes_rec_serial);
    Serial.print("\n");
  }
  else
  {      
    // see if bytes are waiting to be sent to sign
    if (!serial_buffer_empty)
    {
      // send bytes to sign, one chunk at a time
      if (num_bytes_sent_serial < num_bytes_rec_serial)
      {
        // check if "chunk-size" bytes should be sent, or the lesser remainder bytes. 
        if (chunk_size < (num_bytes_rec_serial - num_bytes_sent_serial) )
        {
          // load "chunk-size" bytes from EEPROM into SRAM
          for (unsigned short int i = 0; i < chunk_size; i++)
          {
            sign_packet_buffer[i] = EEPROM.read(num_bytes_sent_serial + i);
          }
          // send chunk
          num_bytes_sent_serial += ( s_sign.write( sign_packet_buffer, chunk_size) ); 
        }
        else
        {        
          // load remaining bytes from EEPROM into SRAM
          for (unsigned short int i = 0; i < (num_bytes_rec_serial - num_bytes_sent_serial); i++)
          {
            sign_packet_buffer[i] = EEPROM.read(num_bytes_sent_serial + i);
          }
          // send chunk
          num_bytes_sent_serial += ( s_sign.write( sign_packet_buffer, (num_bytes_rec_serial - num_bytes_sent_serial)) ); 
          Serial.println("last chunk");
        }
      }
      else
      {
        // done; reset state variables
        serial_buffer_empty = true;
        num_bytes_rec_serial = 0;
        num_bytes_sent_serial = 0;
      }
      
      Serial.print("sent: ");
      Serial.print(num_bytes_sent_serial);
      Serial.print("\n");
    }
  }  



}







