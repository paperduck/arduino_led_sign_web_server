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
 - xxx create functions for repeated code
 - xxx restore alert (including countown) if power goes out
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
//byte mac[] = {
//  0x90, 0xA2, 0xDA, 0x0F, 0x96, 0xBE};
//byte ip[] = {
//  192, 168, 1, 203};
EthernetServer server = EthernetServer(80);
EthernetClient client = EthernetClient();

// TCP communication -----------------------------------
bool                       header_needs_to_be_sent = true;
bool                       tcp_do_send = false;

// SD card -----------------------------------
File                       f; // file to read from SD card
bool                       file_exists = false;
bool                       file_open = false;
byte                       cur_file_exists_attempts = 0;
const byte                 max_file_exists_attempts = 10;
byte                       cur_file_open_attempts = 0;
const byte                 max_file_open_attempts = 10;

// Serial -----------------------------------
SoftwareSerial             s_sign(2,3);
SoftwareSerial             s_pc(5,6);   // pin 4 used for ethernet shield's SD card slave select 
long                       time_since_last_rec_byte = millis() + 60000;
const unsigned short int   chunk_size = 3;
byte                       sign_packet_buffer[chunk_size]; 
byte                       num_bytes_sent_serial = 0; // counter for num bytes sent
byte                       num_bytes_rec_serial = 0;  // counter for num bytes received
byte                       serial_incoming_outgoing_delay = 1000;
unsigned long              time_of_last_incoming_serial = 0;
byte                       num_bytes_last_received_serial = 0;

// diagnostic/debugging -----------------------------------
//short                      memory_available = 0;
//short                      memory_available_min = 9999;

// Parsing, Flags, State -----------------------------------
byte                       cur_byte;
String                     token = "";
unsigned long              cur_line_num = 1;   // 1-based. 0 means no lines yet. (lines of incoming header)
unsigned short int         cur_token_num = 0;  // 1-based. e.g. GET will have token index 1, Filename 2, HTTP version 3.
String                     file_path_name = ""; // file requested + args
char *                     file_path_name_buf; // char* for passing into SD.exists(), SD.open()
boolean                    start_new_token = true;
short int                  index_of; // stores result of String.indexOf()
short int                  index_of2; // for String.substring()
short int                  index_of_decode;

// File stuff -----------------------------------
String                     file_media_type; // a.k.a. MIME type, a.k.a. Content-type

// EEPROM - 1024 bytes -----------------------------------
const byte                 num_bytes_last_received_serial_start = 0;
const byte                 sign_buffer_start = 1; // xxx change to 0 (or whatever) for production
unsigned short int         eeprom_write_counter = 0; // xxx comment out for production

// alert settings -----------------------------------
char                       param; // holds parameter name
String                     sign_text = ""; // param: t
unsigned long              duration_seconds = 1; // param: d
//byte                       method = 0; // param: m
//byte                       color = 0; // param=c
unsigned long              time_of_last_program_ms = millis(); // xxx change this to accomdate power outage (use EEPROM)
bool                       alert_active = false;

// sign values -----------------------------------
//const byte                 color_red[1]       = {0xB0};
//const byte                 color_bright_red   = {0xB1};
//const byte                 color_orange       = {0xB2};
const byte                 sign_packet_header[10] = {
  0x00, 0xFF, 0xFF, 0x00, 0x0B, 0x01, 0xFF, 0x01, 0x30, 0x31};
const byte                 sign_packet_spacer[] = {
  0xEF};
const byte                 sign_packet_color[] = {
  0xB0};
const byte                 sign_packet_method[] = {
  0x13};
const byte                 sing_packet_font[] = {
  0xA2};
const byte                 sign_packet_tail[] = {
  0xFF, 0xFF, 0x00};

/****************************************************************/
void setup()
{
  Serial.begin(9600); // ~300 bytes program space memory. 
  s_sign.begin(2400); // !!! the order that these serials are begun seems to matter !!!
  s_pc.begin(2400); // !!! the order that these serials are begun seems to matter !!!

  // LCD screen
  //  lcd.begin(16,2);

  // Ethernet & SD card
  //pinMode(10, OUTPUT); // xxx why is this here
  //  while ( ! Ethernet.begin(mac) )
  //  {
  //    ;
  //  }
  //  Ethernet.begin(mac, ip);  
  // Read IP address and MAC address from file on SD card
  while ( ! SD.begin(4))
  {
    ;
  }
  bool got_eth_info = false;
  if (SD.exists("/eth.txt"))
  {
    File f = SD.open("/eth.txt", FILE_READ);
    if (f) 
    {
      while(f.available())
      {
        break;
      }
    }
  }
  if (!got_eth_info)
  {
    // use defaults
    // xxx 
  }
  server.begin(); // starts listening for incoming connections

  // EEPROM
  num_bytes_last_received_serial = EEPROM.read(num_bytes_last_received_serial_start);

  // LED
  pinMode(A4, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);

  digitalWrite(A4, HIGH);
  digitalWrite(7, HIGH);
  digitalWrite(8, HIGH);
  digitalWrite(9, HIGH);
} 

/****************************************************************/
// multiline uses same SRAM as inline concatenation
// 'file_media_type' should be a media type, e.g. "image/png", "text/html".
//   http://en.wikipedia.org/wiki/Internet_media_type#Type_image
//
boolean send_http_header(const unsigned long & file_size, const String & file_media_type)
{  
  server.print( "HTTP/1.1 200 OK\r\nContent-Type: " );
  server.print( file_media_type);
  server.print( "\r\nContent-length: " );
  server.print( file_size ); // xxx make sure I don't need to cast to String
  server.print( "\r\n\r\n" );
}

/****************************************************************/
// Returns file media type

String get_file_media_type(const String & file_name)
{
  //  long index_of = file_name.lastIndexOf(F("."));
  //
  //  if (index_of == -1)
  //  {
  //    // no period found, use a default type
  //    return F("text/html");
  //  }
  //  else if (file_name.substring(index_of) == F(".png"))
  //  {
  //    return F("image/png");
  //  }
  //  else 
  //  {
  return "text/html";
  //  }
}

/****************************************************************/
void loop()
{
  if (client)
  {
    //process_client();
    process_client_piecemeal();
  }
  else
  {
    // Check for incoming client.
    client = server.available(); 
  }

  serial_listen();

  if (alert_active)
  {
    if ( (millis() - time_of_last_program_ms) > (duration_seconds * 1000) )
    {
      restore_sign();
    }
  }
}

/****************************************************************/
void serial_listen()
{
  // listen for incoming serial communication
  if (s_pc.available())
  { 
    // read a byte into EEPROM
    if (eeprom_write_counter < 500) // xxx change for production
    {
      EEPROM.write(sign_buffer_start + num_bytes_rec_serial, s_pc.read() );
      eeprom_write_counter++;
    }
    num_bytes_rec_serial ++;
    time_of_last_incoming_serial = millis();
  }
  else
  {      
    // send bytes to sign, one chunk at a time
    if (num_bytes_rec_serial > 0)
    {    
      if (num_bytes_sent_serial < num_bytes_rec_serial)
      {
        // There are bytes that have been received and are waiting in EEPROM to be sent.
        // There is a delay here to allow time for bytes to arrive from PC.
        //   Sending will reset prematurely if bytes are sent too quickly, which would
        //   result in garbage being sent to sign.
        if ( (millis() - time_of_last_incoming_serial) > serial_incoming_outgoing_delay )
        {
          // check if "chunk-size" bytes should be sent, or the lesser remainder bytes. 
          if (chunk_size < (num_bytes_rec_serial - num_bytes_sent_serial) )
          {
            // load "chunk-size" bytes from EEPROM into SRAM
            for (unsigned short int i = 0; i < chunk_size; i++)
            {
              sign_packet_buffer[i] = EEPROM.read(sign_buffer_start + num_bytes_sent_serial + i);
              //Serial.print( String(" ") + String( sign_packet_buffer[i], HEX ) ); 
            }
            // send chunk
            num_bytes_sent_serial += ( s_sign.write( sign_packet_buffer, chunk_size) ); 
          }
          else
          {        
            // load remaining bytes from EEPROM into SRAM
            for (unsigned short int i = 0; i < (num_bytes_rec_serial - num_bytes_sent_serial); i++)
            {
              sign_packet_buffer[i] = EEPROM.read(sign_buffer_start + num_bytes_sent_serial + i);
              //Serial.print( String(" ") + String( sign_packet_buffer[i], HEX ) ); 
            }
            // send chunk
            num_bytes_sent_serial += ( s_sign.write( sign_packet_buffer, (num_bytes_rec_serial - num_bytes_sent_serial)) ); 
          }
        }
      }
      else
      {
        // all received bytes have been sent, so reset 
        if (eeprom_write_counter < 500) // xxx remove for production
        {
          eeprom_write_counter ++;
          EEPROM.write(num_bytes_last_received_serial_start, num_bytes_sent_serial);
        }
        num_bytes_rec_serial = 0;
        num_bytes_sent_serial = 0;
      }
    }
  }  
}

/****************************************************************/
// Purpose:
// Revert sign to the message that was programmed using manufacturer's software.
void restore_sign()
{
  // this just triggers a re-reading of bytes already stored in memory
  num_bytes_rec_serial = EEPROM.read(num_bytes_last_received_serial_start);  
  alert_active = false;
}

/****************************************************************/
// Purpose:
//
void program_sign(const String & sign_text)
{  
  s_sign.write(sign_packet_header, 10);
  s_sign.write(sign_packet_spacer, 1);
  s_sign.write(sign_packet_color, 1);
  s_sign.write(sign_packet_spacer, 1);
  s_sign.write(sing_packet_font, 1);
  s_sign.print(sign_text);
  s_sign.write(sign_packet_tail, 3);

  time_of_last_program_ms = millis();
  alert_active = true;
}

/****************************************************************/
void process_client_piecemeal()
{ 
  if (client)
  {
    if (!tcp_do_send)
    {
      // receive data from client.    
      cur_byte = client.read();

      if (cur_byte == 0xFF) // 0xFF == EOF == -1 as char
      {
        tcp_reset_receive();
      }
      else
      {
        Serial.print((char)cur_byte);

        // if whitespace
        if (char(cur_byte) == char(' '))
        {
          start_new_token = true;
          //token.toLowerCase(); // 64 bytes of program space memory

          // process token that just finished arriving
          switch (cur_line_num)
          {
          case 1: // cur_line_num
            switch (cur_token_num)
            {
            case 2:            
              // This token should be the file requested.

              if (token == "/")
              {
                token = "/index.htm"; 
              }

              // parse arguments, if any
              index_of = token.indexOf("?");
              if (index_of > -1)
              {
                // there are arguments
                // grab just the path+filename
                // first, char buffer
                file_path_name_buf = (char*)malloc((index_of * sizeof(char)) + 1);
                token.toCharArray( file_path_name_buf, index_of + 1 );              
                // second, String
                file_path_name = token.substring(0, index_of);

                // url decoding
                decode_url(file_path_name);

                // assume all args are there and in a certain order:
                //   t   = text
                //   d   = duration
                //   m   = method
                //   c   = color
                //   ...

                // text ------------------
                param = (token[index_of + 1]);
                //if (param == 't')
                //{
                index_of += 3;
                index_of2 = token.indexOf('&');
                sign_text = token.substring(index_of, index_of2);
                // url decoding
                decode_url(sign_text);
                //}

                // duration ---------------
                index_of = index_of2 + 1;
                param = (token[index_of]);
                //if (param == 'd')
                //{
                index_of += 2;
                index_of2 = token.indexOf('&', index_of);
                if (index_of2 > -1)
                {
                  duration_seconds = token.substring(index_of, index_of2).toInt();
                  duration_seconds = 3;
                }
                else
                {
                  duration_seconds = token.substring(index_of).toInt();
                  //duration_seconds = 1;
                }
                //}
                program_sign(sign_text);
              }
              else
              {
                // no arguments 
                file_path_name_buf = (char*)malloc((token.length() * sizeof(char)) + 1);
                token.toCharArray( file_path_name_buf, token.length() + 1 );              
                file_path_name = token;
              }
              break;

            case 3:
              // protocol version should be followed by CRLF, not (SP)
              // xxx    400 (Bad Request) error
              fail_request(F("400")); 
              break;

            default: // cur_token_num
              break;  
            }
            break;

          default: // cur_line_num
            break;
          }
          start_new_token = true; // cur byte is a space, so no longer 
        }
        else if( char(cur_byte) == char('\r') )
        {
          if (cur_line_num == 1 && cur_token_num == 3)
          {
            // This token should be protocol (HTTP) version 
            if (token.startsWith("HTTP"))
            {
              // good to go 
              tcp_reset_receive();
              tcp_do_send = true;
            }
            else
            {
              ; // xxx    505  (HTTP Version Not Supported) 
              fail_request(F("505")); 
            }          
          }
          //          else
          //          {
          //            ; // xxx parse other lines of request as needed
          //          }
        }
        else if(char(cur_byte) == char('\n'))
        {        
          cur_line_num++; // increment the \n
          start_new_token = true; // reset just in case spaces at end of last line
          cur_token_num = 0; // back to first token
        }
        else
        {
          // non-whitespace, so append to token
          if (start_new_token)
          {
            // starting a new token
            cur_token_num++;
            token = "";
            start_new_token = false;
          }
          token += char(cur_byte);
        }
      }   
    }
    else
    {
      // sending data to client
      if (!file_exists)
      {
        // attempt to check if file exists, if attempts not already maxed
        if (cur_file_exists_attempts < max_file_exists_attempts)
        {
          // check if file exists     
          cur_file_exists_attempts ++;
          if (SD.exists(file_path_name_buf))
          {
            file_exists = true;
          }
        }
        else
        {
          // existence check attempts exceeded
          // 404   
          fail_request(F("404")); 
        }
      }
      else
      {
        // file exists, next step is try to open it              
        if (!file_open)
        {
          if (cur_file_open_attempts < max_file_open_attempts)
          {
            // attempt to open file
            cur_file_open_attempts ++;
            f = SD.open(file_path_name_buf, FILE_READ);
            if (f) // evaluates to false if file couldn't be opened ~ http://arduino.cc/en/Reference/SDopen
            {
              file_open = true;
            }
          }
          else
          {
            // file open attempts exceeded
            fail_request(F("couldn't open file")); 
          }
        }
        else
        {        
          // file is ready

          if (header_needs_to_be_sent)
          {
            // send header
            send_http_header(f.size(), get_file_media_type(file_path_name));
            header_needs_to_be_sent = false;
          }

          // send a byte
          if (f.available())
          {
            // read a byte, send it
            //cur_byte = f.read();
            //server.write( cur_byte );
            server.write( f.read() );
          }
          else
          {
            // nothing to read, so close file and finish up response
            tcp_reset_send();
            tcp_disconnect();
          }
        }
      }
    }     
  }
  else
  {
    // No client. 
    // Check for incoming client.
    client = server.available(); 
  }
}

/****************************************************************/
// order matters
void decode_url(String & s)
{
  decode_url_1(s, "+", ' ', 0);
  //decode_url_1(s, "%2B", '+', 2);
  //decode_url_1(s, "%5E", '^', 2);
  //decode_url_1(s, "%2C", ',', 2);
  //decode_url_1(s, "%3B", ';', 2);
  //decode_url_1(s, "%3A", ':', 2);
  //decode_url_1(s, "%25", '%', 2);
}

/****************************************************************/
void decode_url_1(String & s, const String & old, const char & new_, const unsigned short int & extra_count)
{
  index_of_decode = sign_text.indexOf(old);
  while(index_of_decode > -1)
  {
    //sign_text.replace("%2B", "+"); // ~800 bytes program memory
    if (extra_count > 0)
    {
      sign_text.remove(index_of_decode, extra_count);
    }
    sign_text[index_of_decode] = new_;
    index_of_decode = sign_text.indexOf(old);
  }
}

/****************************************************************/
void fail_request(const String & msg)
{
  send_http_header(10, "text/plain"); // xxx This takes roughly 50 bytes program memory
  server.print(msg); // xxx this takes about 16 bytes program memory
  tcp_reset_send();
  tcp_disconnect();
}

/****************************************************************/
void tcp_reset_receive()
{
  // reset for listening
  token = "";
  cur_token_num = 0;
  cur_line_num = 1;
  start_new_token = true;
}

/****************************************************************/
void tcp_reset_send()
{
  free(file_path_name_buf); // super-important memory deallocation
  f.close();  
  header_needs_to_be_sent = true;
  cur_file_exists_attempts = 0;
  cur_file_open_attempts = 0;
  file_exists = false;
  file_open = false;
  tcp_do_send = false;
}

/****************************************************************/
void tcp_disconnect()
{   
  // disconnect client 
  if (client.connected())
  {
    client.stop();  // disconnect the client
  }
}

/****************************************************************/
//void process_client()
//{
//  //  lcd_print(15, 1, "c");
//  start_new_token = false;
//  cur_line_num = 1;
//  cur_token_num = 0;
//  // read in all data from client, then process it.
//  while (client.available())
//  {
//    cur_byte = client.read();
//    //te(cur_byte);
//
//    if (char(cur_byte) == char(' '))
//    {
//      // space
//      if (start_new_token)
//      {
//        // current token ended
//        start_new_token = false;
//        switch (cur_line_num)
//        {
//        case 1:
//          switch(cur_token_num)
//          {
//          case 0:
//            // request type (GET, POST, ...)
//            request_type = token;
//            break; 
//          case 1:
//            // file requested
//            file_path_name = token;
//            file_path_name.toLowerCase();
//            if (file_path_name == "/" || file_path_name == "\\")
//            {
//              file_path_name = "index.htm";
//            }
//            break; 
//          case 2:
//            // http version
//            // This probably won't be reached because the last token should end with carriage return
//            //http_version_client = token;
//            break; 
//          default:
//            // 400 (Bad Request)
//            // xxx
//            break; 
//          }
//          break;
//        default:
//          // xxx   if GET, ignore rest of lines but need to parse arguments from file path
//          // xxx   if POST, need to get body
//          break;
//        }
//      }
//    }
//    else if (char(cur_byte) == char('\r'))
//    {
//      // carriage return
//      if (start_new_token)
//      {
//        // current token ended
//        start_new_token = false;
//        switch (cur_line_num)
//        {
//        case 1:
//          switch(cur_token_num)
//          {
//          case 2:
//            //http_version_client = token;
//            break; 
//          default:
//            // 400 (Bad Request)
//            // xxx
//            break; 
//          }
//        default:
//          break;
//        }
//      }
//    }
//    else if (char(cur_byte) == char('\n'))
//    {
//      // newline
//      cur_line_num ++;
//      cur_token_num = 0;
//    }
//    else
//    {
//      // non-whitespace 
//      if (!start_new_token)
//      {
//        // starting a new token
//        token = "";
//        cur_token_num ++;
//        // check length of token, make sure it's not too long
//        // xxx
//      }
//      start_new_token = true;
//      token += String(char(cur_byte));
//    }
//  }// end while
//
//    // parse out arguments in file path
//  index_of = file_path_name.indexOf("?", 0);
//  if (index_of > -1)
//  {
//    // extract arguments  xxx
//    file_path_name = file_path_name.substring(0, index_of); 
//  }
//
//  // retrieve the requested file
//  file_path_name_buf = (char*)malloc( (sizeof(char) * file_path_name.length()) + 1 );
//  file_path_name.toCharArray(file_path_name_buf, (file_path_name.length() + 1));
//
//  if (SD.exists(file_path_name_buf)) // costs 119 bytes ?
//  {
//    //    lcd_print(4, 0, "+");  
//    /*File*/ f = SD.open(file_path_name_buf, FILE_READ);  // costs 31 bytes ?     
//    if (f.available())
//    {
//      //      lcd_print(6, 0, "+");
//
//      // xxx determine content-type based on filename
//      // send header
//      send_http_header(f.size(), get_file_media_type(file_path_name), 200);
//      // send file
//      while (f.available())
//      {
//        temp_byte = f.read();
//        server.write(temp_byte);
//        //Serial.write(temp_byte); // debugging        
//      }
//    }
//    else
//    {
//      // xxx  204 No Content 
//      //      lcd_print(6, 0, "-");
//      send_http_header(3,  F("text/plain"), 204 );
//      server.print(F("204")); // no content
//    }
//    // close file 
//    f.close();
//  }
//  else
//  {
//    // xxx  404 Not Found
//    //    lcd_print(4, 0, "-");
//    send_http_header(3,  F("text/plain"), 404 );
//    server.print(F("404")); // not found
//  }
//  //   disconnect client, if there is no undread data from client.
//  if (client.connected())
//  {
//    client.stop(); 
//  }
//  free(file_path_name_buf); // super-important memory deallocation
//  token = "";
//  request_type = "";
//  file_path_name = "";
//  // xxx any others?
//}

/****************************************************************/











































