/*
LED Adapter
 Date Created:  2014-11-19
 Using COM 6 on CB workstation
 
 TODO:
 - xxx consider a message queue that cycles so user can see every message
 - xxx continuously check ethernet_on flag, try to connect if false
 - xxx likewise, continuously check SD "enabled" flag
 - xxx consider replacing all String class uses with char arrays/pointers
 - xxx triple-check memory allocation + deallocation
 - xxx retry upon 404, 204 a certain number of times
 - xxx create functions for repeated code
 - xxx restore alert (including countown) if power goes out
 - xxx check for unused variables
 - xxx don't store entire sign text in SRAM, go straight to/from SD or EEPROM
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
const byte                 sd_pin = 4;
File                       f; // file to read from SD card
bool                       file_exists = false;
bool                       file_open = false;
char *                     sd_ethernet_file = "/ETH.TXT";
//const char *               old_sign_packet_file = "/tmp/oldpacket";
char *                     sd_alert_text_file = "/TMP/ALERT.TXT";
// this is the entire sign packet, not just the message text:
char *                     sd_normal_sign_file = "/TMP/NORMAL.TXT"; 
char *                     sd_temp_folder = "/TMP/";

// Serial -----------------------------------
SoftwareSerial             serial_sign(2,3);
SoftwareSerial             serial_pc(5,6);   // pin 4 used for ethernet shield's SD card slave select 
const byte                 chunk_size = 3;
byte                       sign_buffer[chunk_size]; 
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
byte                       cur_line_num = 1;   // 1-based. 0 means no lines yet. (lines of incoming header)
byte                       cur_token_num = 0;  // 1-based. e.g. GET will have token index 1, Filename 2, HTTP version 3.
String                     file_path_name = ""; // file requested + args
char *                     file_path_name_buf; // char* for passing into SD.exists(), SD.open()
boolean                    start_new_token = true;
short int                  index_of; // stores result of String.indexOf(). Must be signed to hold -1.
short int                  index_of2; // for String.substring()
short int                  index_of_decode;
bool                       in_request_alert_text = false;

// File stuff -----------------------------------
String                     file_media_type; // a.k.a. MIME type, a.k.a. Content-type

// EEPROM - 1024 bytes -----------------------------------
const byte                 num_bytes_last_received_serial_start = 0;
const byte                 sign_buffer_start = 1;
unsigned short int         eeprom_write_counter = 0; // xxx comment out for production

// alert settings -----------------------------------
char                       param; // holds parameter name
//String                     sign_text = ""; // param: t


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
  // LED
  pinMode(A4, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);

  digitalWrite(A4, HIGH);
  digitalWrite(7, HIGH);
  digitalWrite(8, HIGH);
  //  digitalWrite(9, HIGH);
  //  delay(100);
  //  digitalWrite(A4, LOW);
  //  digitalWrite(7, LOW);
  //  digitalWrite(8, LOW);
  //  digitalWrite(9, LOW);

  Serial.begin(9600); // ~300 bytes program space memory. 
  serial_sign.begin(2400); // !!! the order that these serials are begun seems to matter !!!
  serial_pc.begin(2400); // !!! the order that these serials are begun seems to matter !!!

  // LCD screen
  //  lcd.begin(16,2);

  // Initialize SD card
  SD.begin(sd_pin);
  // make /tmp/ directory if it doesn't already exist
  if (! SD.exists(sd_temp_folder) )
  {
    SD.mkdir(sd_temp_folder);
  }

  // Initialize ethernet connection
  byte           mac[6];
  byte           ip[4];
  String         eth_tmp = ""; // line
  bool           got_mac = false;
  bool           got_ip = false;
  //  bool           line_2_blank = false;
  //
  //  digitalWrite(A4, HIGH);
  //  digitalWrite(7, HIGH);
  if (SD.exists( sd_ethernet_file ))
  {
    f = SD.open( sd_ethernet_file, FILE_READ);
    if (f) 
    {
      // read in mac (line 1)  -----------------------------
      index_of = 0;
      while(f.available())
      {
        cur_byte = f.read();
        if (cur_byte == '\n')
        {
          break;
        }
        else
        {
          eth_tmp += (String)(char)cur_byte;
          index_of ++;
          //          if (index_of > 20)
          //          {
          //            break;
          //          }
        }
      }

      // parse mac  -----------------------------
      index_of = eth_tmp.indexOf("-", 0); 
      for (byte i = 0; i < 6; i++)
      {
        mac[i] = hex_to_byte( eth_tmp.substring(index_of - 2, index_of) );
        index_of += 3;
      }
      got_mac = true;

      //      Serial.println("mac: ");
      //      for (byte m = 0; m < 6; m++)
      //      {
      //        Serial.println( mac[m] );
      //      }

      // check for IP (line 2)  -----------------------------
      index_of = 0;
      eth_tmp = "";
      while(f.available())
      {
        cur_byte = f.read();
        if (cur_byte == '\n')
        {
          break;
        }
        else
        {
          eth_tmp += (String)(char)cur_byte;
          index_of ++;
          //          if (index_of > 20)
          //          {
          //            break;
          //          }
        }
      }

      // parse ip -----------------------------
      index_of = eth_tmp.indexOf(".", 0); 
      for (byte i = 0; i < 4; i++)
      {
        ip[i] = byte( eth_tmp.substring(index_of - 3, index_of).toInt() );
        index_of += 4;
      }
      got_ip = true;

      //      Serial.println("ip: ");
      //      for (byte i = 0; i < 4; i++)
      //      {
      //        Serial.println( ip[i] );
      //      }

      if (got_mac)
      {
        digitalWrite(A4, LOW); 
        if (got_ip)
        {
          Ethernet.begin(mac, ip);  
          digitalWrite(7, LOW);
        }
        else
        {
          //          Serial.println("attempting auto ip");
          if ( 0/*xxx Ethernet.begin(mac) */ ) // 4060 bytes program memory
          {
            digitalWrite(8, LOW);
          }
          else
          {
            //            Serial.println("auto ip failed");
          }
        }
      }
      else
      {
        //        Serial.println("didn't get mac");
      }
      f.close();
    }
    else
    {
      //      Serial.println("couldn't open file");
    }
  }
  server.begin(); // starts listening for incoming connections

  // EEPROM
  num_bytes_last_received_serial = EEPROM.read(num_bytes_last_received_serial_start);

  // auto-reset sign
  reset_sign_sd();

}

/****************************************************************/
// s is a String with length of 2.
//
byte hex_to_byte( const String & s )
{
  byte b = 0;

  switch( (char)s[0] )
  {
  case '1':
    b = 16;
    break;
  case '2':
    b = 32;
    break;
  case '3':
    b = 48;
    break;
  case '4':
    b = 64;
    break;
  case '5':
    b = 80;
    break;
  case '6':
    b = 96;
    break;
  case '7':
    b = 112;
    break;
  case '8':
    b = 128;
    break;
  case '9':
    b = 144;
    break;
  case 'A':
    b = 160;
    break;
  case 'B':
    b = 176;
    break;
  case 'C':
    b = 192;
    break;
  case 'D':
    b = 208;
    break;
  case 'E':
    b = 224;
    break;
  case 'F':
    b = 240;
    break;
  default:
    break; 
  }

  switch( (char)s[1] )
  {
  case '1':
    b += 1;
    break;
  case '2':
    b += 2;
    break;
  case '3':
    b += 3;
    break;
  case '4':
    b += 4;
    break;
  case '5':
    b += 5;
    break;
  case '6':
    b += 6;
    break;
  case '7':
    b += 7;
    break;
  case '8':
    b += 8;
    break;
  case '9':
    b += 9;
    break;
  case 'A':
    b += 10;
    break;
  case 'B':
    b += 11;
    break;
  case 'C':
    b += 12;
    break;
  case 'D':
    b += 13;
    break;
  case 'E':
    b += 14;
    break;
  case 'F':
    b += 15;
    break;
  default:
    break; 
  }

  return b;
}


/****************************************************************/
// multiline uses same SRAM as inline concatenation
// 'file_media_type' should be a media type, e.g. "image/png", "text/html".
//   http://en.wikipedia.org/wiki/Internet_media_type#Type_image
//
boolean send_http_header(const unsigned long & file_size, const String & file_media_type)
{  
  server.print( F("HTTP/1.1 200 OK\r\nContent-Type: ") );
  server.print( file_media_type);
  server.print( F("\r\nContent-length: ") );
  server.print( file_size ); // xxx make sure I don't need to cast to String
  server.print( F("\r\n\r\n") );
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
  return F("text/html");
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

  serial_listen_sd();

  if (alert_active)
  {
    if ( (millis() - time_of_last_program_ms) > (duration_seconds * 1000) )
    {
      reset_sign_sd();
    }
  }
}

/****************************************************************/
void serial_listen_sd()
{  
  if (serial_pc.available())
  {
    // make sure file object is not in use
    if (!f)
    {
      if (SD.exists(sd_normal_sign_file))
      {
        SD.remove(sd_normal_sign_file);
      }
      f = SD.open(sd_normal_sign_file, FILE_WRITE);
      if (f)
      {
        while ( serial_pc.available() )
        {
          f.print( serial_pc.read() );
        }
        f.close();
      }
      else
      {
        Serial.println("serial_listen_sd() couldn't open file"); 
      }
    }
  }
}

/****************************************************************/
void serial_listen()
{
  // listen for incoming serial communication
  if (serial_pc.available())
  { 
    // read a byte into EEPROM
    EEPROM.write(sign_buffer_start + num_bytes_rec_serial, serial_pc.read() );
    //    eeprom_write_counter++;
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
            for (byte i = 0; i < chunk_size; i++)
            {
              sign_buffer[i] = EEPROM.read(sign_buffer_start + num_bytes_sent_serial + i);
              //Serial.print( String(" ") + String( sign_buffer[i], HEX ) ); 
            }
            // send chunk
            num_bytes_sent_serial += ( serial_sign.write( sign_buffer, chunk_size) ); 
          }
          else
          {        
            // load remaining bytes from EEPROM into SRAM
            for (byte i = 0; i < (num_bytes_rec_serial - num_bytes_sent_serial); i++)
            {
              sign_buffer[i] = EEPROM.read(sign_buffer_start + num_bytes_sent_serial + i);
              //Serial.print( String(" ") + String( sign_buffer[i], HEX ) ); 
            }
            // send chunk
            num_bytes_sent_serial += ( serial_sign.write( sign_buffer, (num_bytes_rec_serial - num_bytes_sent_serial)) ); 
          }
        }
      }
      else
      {
        // all received bytes have been sent, so reset 
        //        eeprom_write_counter ++;
        EEPROM.write(num_bytes_last_received_serial_start, num_bytes_sent_serial);
        num_bytes_rec_serial = 0;
        num_bytes_sent_serial = 0;
      }
    }
  }  
}

/****************************************************************/
// Purpose:
// Program sign to the message that was set using manufacturer's software.
void reset_sign()
{
  // this just triggers a re-reading of bytes already stored in memory
  num_bytes_rec_serial = EEPROM.read(num_bytes_last_received_serial_start);  
  alert_active = false;
}

/****************************************************************/
// Purpose:
// Program sign to the message that was set using manufacturer's software.
void reset_sign_sd()
{
  program_sign_sd(sd_normal_sign_file);
}

/****************************************************************/
// Purpose:
//
void program_sign(const String & sign_text)
{  
  serial_sign.write(sign_packet_header, 10);
  serial_sign.write(sign_packet_spacer, 1);
  serial_sign.write(sign_packet_color, 1);
  serial_sign.write(sign_packet_spacer, 1);
  serial_sign.write(sing_packet_font, 1);  
  serial_sign.print(sign_text);  
  serial_sign.write(sign_packet_tail, 3);

  time_of_last_program_ms = millis();
  alert_active = true;
}

/****************************************************************/
// Purpose:
//
void program_sign_sd(const char * fname)
{  
  f = SD.open(fname, FILE_READ);
  if (f)
  {        
    serial_sign.write(sign_packet_header, 10);
    serial_sign.write(sign_packet_spacer, 1);
    serial_sign.write(sign_packet_color, 1);
    serial_sign.write(sign_packet_spacer, 1);
    serial_sign.write(sing_packet_font, 1);      
    while(f.available())
    {
      serial_sign.write(f.read());
    }    
    serial_sign.write(sign_packet_tail, 3);
    f.close();
    time_of_last_program_ms = millis();
    alert_active = true;
  }
  else
  {
    // xxx error
    Serial.println("prog sd failed");
  }
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
              Serial.println("token:");
              Serial.println(token);

              // This token should be the file requested.
              if (token == "/")
              {
                token = F("/index.htm"); 
                break;
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
                if (index_of == -1 || index_of2 == -1)
                {
                  // xxx error
                }
                //sign_text = token.substring(index_of, index_of2);
                // write sign text to SD
                //  //  //                if (SD.exists(sd_alert_text_file))
                //  //  //                {
                //  //  //                  SD.remove(sd_alert_text_file); 
                //  //  //                }                
                //  //  //                f = SD.open(sd_alert_text_file, FILE_WRITE);
                //  //  //                if (f)
                //  //  //                {
                //  //  //                  f.print( token.substring(index_of, index_of2) );
                //  //  //                }      
                //  //  //                else
                //  //  //                {
                //  //  //                  Serial.println("SD.open() failed while loading token into alert-text file"); 
                //  //  //                }
                //  //  //                f.close();   

                // url decoding
                //decode_url(sign_text);
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
                //program_sign(sign_text);
                program_sign_sd(sd_alert_text_file);
              }
              else
              {
                Serial.println("no args");
                // no arguments 
                file_path_name_buf = (char*)malloc((token.length() * sizeof(char)) + 1);
                token.toCharArray( file_path_name_buf, token.length() + 1 );              
                file_path_name = token;
              }
              break;

            case 3:
              // protocol version should be followed by CRLF, not (SP)
              // 400 (Bad Request) error
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
            if (token.startsWith( F("HTTP") ))
            {
              // good to go 
              tcp_reset_receive();
              tcp_do_send = true;
            }
            else
            {
              // 505  (HTTP Version Not Supported) 
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
          // non-whitespace
          if (cur_line_num == 1 && cur_token_num == 2)
          {
            // If the alert text is an argument here, then write it directly to the SD card
            //   instead of saving it to a token.
            if (in_request_alert_text)
            {
              // write to SD card
              if (SD.exists(sd_alert_text_file))
              {
                SD.remove(sd_alert_text_file); 
              }   
              f = SD.open(sd_alert_text_file, FILE_WRITE);
              if (f)
              {
                while ( cur_byte != 0xFF )
                {
                  f.print( char(cur_byte) );
                  cur_byte = client.read();  
                  if ( char(cur_byte) == char('&') )
                  {
                    // end of alert text
                    in_request_alert_text = false;
                    break;
                  }                 
                }
                f.close();
              }
              else
              {
                Serial.println("SD.open failed 1"); 
              }
            }
            else
            {
              if ( char(cur_byte) == char('?') )
              {
                // assume first parameter is: "t="
                cur_byte = client.read();
                cur_byte = client.read();
                // beginning of alert text
                in_request_alert_text = true;   
                token += "?t=yyy&";             
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
    }
    else
    {
      // sending data to client
      if (!file_exists)
      {
        // check if file exists     
        if (SD.exists(file_path_name_buf))
        {
          file_exists = true;
        }
        else
        {
          // maybe SD card needs to be restarted
          SD.begin(sd_pin);
          if (SD.exists(file_path_name_buf))
          {
            file_exists = true;
          }
          else
          {
            // 404   
            fail_request(F("404")); 
          }
        }
      }
      else
      {
        // file exists, next step is try to open it              
        if (!file_open)
        {
          // attempt to open file
          f = SD.open(file_path_name_buf, FILE_READ);
          if (f) // evaluates to false if file couldn't be opened ~ http://arduino.cc/en/Reference/SDopen
          {
            file_open = true;
          }
          else
          {
            // file open attempts exceeded
            fail_request("couldn't open file"); 
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
  //  decode_url_1(s, "%2B", '+', 2);
  //  decode_url_1(s, "%5E", '^', 2);
  //  decode_url_1(s, "%2C", ',', 2);
  //  decode_url_1(s, "%3B", ';', 2);
  //  decode_url_1(s, "%3A", ':', 2);
  //  decode_url_1(s, "%25", '%', 2);
}

/****************************************************************/
// replace every instance of 'old' in 's' with 'new_'.
// 'extra_count' is difference in number of characters between 'old' and 'new_'.
void decode_url_1(String & s, const String & old, const char & new_, const byte & extra_count)
{
  index_of_decode = s.indexOf(old);
  while(index_of_decode > -1)
  {
    //sign_text.replace("%2B", "+"); // ~800 bytes program memory
    if (extra_count > 0)
    {
      s.remove(index_of_decode, extra_count);
    }
    s[index_of_decode] = new_;
    index_of_decode = s.indexOf(old);
  }
}

/****************************************************************/
void fail_request(const String & msg)
{
  send_http_header(10, F("text/plain") ); // xxx This takes roughly 50 bytes program memory
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
  //  cur_file_exists_attempts = 0;
  //  cur_file_open_attempts = 0;
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









