// PREPARING THE ACCESS TO THE NODEMCU'S FILE SYSTEM  = = = = = = = = = = = 
#include <FS.h>

// PREPARING RF AND WEB SERVER  = = = = = = = = = = = = = = = = = = = = = =
#include <RCSwitch.h>
RCSwitch mySwitch = RCSwitch();


// PREPARING WIFI AND WEB SERVER  = = = = = = = = = = = = = = = = = = = = =
// Load Wi-Fi library
#include <ESP8266WiFi.h>

// Default network credentials
String ssid     = "YOUR_SSID";
String password = "YOUR_PASSWORD";

// Set web server port number to 80
WiFiServer server(80);
WiFiClient client;

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String output5State = "off";
String output4State = "off";

// Assign output variables to GPIO pins
const int output5 = 5;
const int output4 = 4;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;
// Control whether the web page was refreshed already or not.
int pageRefreshed = 0;


// PREPARING SHADES INFORMATION = = = = = = = = = = = = = = = = = = = = = =
// Action starting time
unsigned long actionStartTime = 0;
//shade definition arrays: [UP RF CODE, DOWN RF CODE, STOP RF CODE]
//const int PROGMEM KITCHEN01     = 101;
//const int PROGMEM KITCHEN02     = 102;
//const int PROGMEM ATELIER       = 103;
//const int PROGMEM LIVING01      = 104;
//const int PROGMEM LIVING02      = 105;
//const int PROGMEM LIVING03      = 106;
//const int PROGMEM SUITE01       = 201;
//const int PROGMEM SUITE02       = 202;
//const int PROGMEM OFFICE        = 203;
const int PROGMEM ROOM_TI[]       = {5588888, 5599999, 5592483};
//const int PROGMEM WC            = 205;
//const int PROGMEM ROOM_GUGA     = 206;
const int PROGMEM ALL_SHADES    = 999;
const int PROGMEM UP_ACTION     = 0;
const int PROGMEM DOWN_ACTION   = 1;
const int PROGMEM STOP_ACTION   = 2;
const int PROGMEM ACTION_DURATION = 20000;
const int PROGMEM PORTAO_PEDONAL  = 5577777;
const int PROGMEM PORTAO_RAMPA    = 5566666;

  
void setup() {
  Serial.begin(115200);
  
  // PREPARING THE ACCESS TO THE NODEMCU'S FILE SYSTEM  = = = = = = = = = =
  SPIFFS.begin();

  
  // PREPARING RF = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
  // Transmitter is connected to Arduino Pin #0  
  mySwitch.enableTransmit(0);  // Optional set pulse length.
  // mySwitch.setPulseLength(320);
  
  // Optional set protocol (default is 1, will work for most outlets)
  // mySwitch.setProtocol(2);
  
  // Optional set number of transmission repetitions.
  // mySwitch.setRepeatTransmit(15);



  // PREPARING WIFI AND WEB SERVER  = = = = = = = = = = = = = = = = = = = = 

  // Check if there are credentials saved in FLASH
  if(SPIFFS.exists("/credentials"))
  {
    if(File file = SPIFFS.open("/credentials", "r"))
    {
      String s = file.readStringUntil('\n');
      String p = file.readStringUntil('\n');
      if((s.length() > 0) && (p.length() > 0))
      {
        ssid = s;
        password = p;
      }
      file.close();
    }
  }
  else
  {
    if(File file = SPIFFS.open("credentials", "w"))
    {
      file.println(ssid);
      file.println(password);
      file.close();
    }
  }
  Serial.print("ssid: "); Serial.print(ssid); Serial.print(" pwd: "); Serial.println(password);
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.print("WiFi connected to: ");Serial.println(WiFi.SSID());
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
  server.begin();

}

void loop() {

  // SHADE ACTION MANAGEMENT  = = = = = = = = = = = = = = = = = = = = = = =
  if ( actionStartTime > 0 && (millis() - actionStartTime >= ACTION_DURATION))
  {
    Serial.println("STOPPING ALL SHADES");
    Serial.print("current time: ");Serial.print(millis()); Serial.print("; action start time: "); Serial.print(actionStartTime); Serial.print("; diff: "); Serial.println(millis()-actionStartTime);
    stop_all_shades();
    actionStartTime = 0;
  }

  
  // WIFI AND WEB SERVER  = = = = = = = = = = = = = = = = = = = = = = = = =
  client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= timeoutTime) { // loop while the client's connected
      currentTime = millis();         
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // triggering the requested shades action
            if (header.indexOf("GET /ROOM_TI/up") >= 0) {
              Serial.println("ROOM_TI STOP");
              delay(3000);
              Serial.println("ROOM_TI UP");
              trigger_shade(ROOM_TI, UP_ACTION);
              actionStartTime = currentTime;
              pageRefreshed = 0;
            } else if (header.indexOf("GET /ROOM_TI/down") >= 0) {
              Serial.println("ROOM_TI STOP");
              delay(3000);
              Serial.println("ROOM_TI DOWN");
              trigger_shade(ROOM_TI, DOWN_ACTION);
              actionStartTime = currentTime;
              pageRefreshed = 0;
            } else if (header.indexOf("GET /ROOM_TI/stop") >= 0) {
              Serial.println("ROOM_TI STOP");
              trigger_shade(ROOM_TI, STOP_ACTION);
              pageRefreshed = 0;
            } else if (header.indexOf("GET /PORTAO_PEDONAL/abrir") >= 0) {
              Serial.println("PORTAO PEDONAL OPEN");
              open_gate(PORTAO_PEDONAL);
              pageRefreshed = 0;
            } else if (header.indexOf("GET /PORTAO_RAMPA/abrir") >= 0) {
              Serial.println("PORTAO RAMPA OPEN");
              open_gate(PORTAO_RAMPA);
              delay(200);
              open_gate(PORTAO_RAMPA);  // this is needed for actually open this gate.
              pageRefreshed = 0;
            } else if (header.indexOf("GET /WIFI/") >= 0) {
              Serial.println("NEW WIFI SETTINGS");
              set_wifi(header);
              pageRefreshed = 0;
            }
            

            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<meta charset=\"UTF-8\">");
            if(actionStartTime>0 && !pageRefreshed)
            {
              client.println("<meta http-equiv=\"Refresh\" content=\"0; url=/\"/>");
              pageRefreshed = 1;
            }
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 30px;");
            client.println("text-decoration: none; font-size: 20px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");
            
            // Web Page Heading
            client.println("<body>");
            client.println("<h2>PORTÕES</h2>");

            // Display the buttons to control the front gates
            client.println("<h3>PORTÃO PEDONAL</h3>");
            client.println("<p><a href=\"/PORTAO_PEDONAL/abrir\"><button class=\"button\">ABRIR</button></a></p>");
            client.println("<h3>PORTÃO RAMPA</h3>");
            client.println("<p><a href=\"/PORTAO_RAMPA/abrir\"><button class=\"button\">ABRIR</button></a></p>");
            
            // Display the buttons to control the shades.  
            client.println("<h2>ESTORES</h2>");
            client.println("<p><h3>QUARTO DA TI</b></h3>");
            client.println("<p><a href=\"/ROOM_TI/up\"><button class=\"button\">ABRIR</button></a>&nbsp;&nbsp;&nbsp;");
            client.println("<a href=\"/ROOM_TI/down\"><button class=\"button\">FECHAR</button></a>&nbsp;&nbsp;&nbsp;");
            client.println("<a href=\"/ROOM_TI/stop\"><button class=\"button\">PARAR</button></a></p>");
//            client.println("<!--");
//            client.println("<h3>TODOS</h3>");
//            client.println("<p><a href=\"/\"><button class=\"button2\">ABRIR</button></a>&nbsp;&nbsp;&nbsp;");
//            client.println("<a href=\"/\"><button class=\"button2\">FECHAR</button></a>&nbsp;&nbsp;&nbsp;");
//            client.println("<a href=\"/\"><button class=\"button2\">PARAR</button></a></p>");
//            client.println("-->");

            // Display the form for setting a new wifi access
//            client.println("<h2>CONNECT TO WIFI</h2>");
//            client.println("<p><h3>ENTER SSID AND PASSWORD</b></h3>");
//            client.println("<form name=wifi method=get action=\"/WIFI/\">");
//            client.println("<input type=text name=ssid placeholder=ssid /><input type=text name=password placeholder=password />");
//            client.println("<p><button class=\"button\">LIGAR</button></a></p>");
//            client.println("</form>");

            // close the html markup
            client.println("</body></html>");
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}


void set_wifi(String url)
{
  // GET /WIFI/?ssid=2bf&password=2bfguest HTTP/1.1
  int s_pos=url.indexOf("ssid=");
  int p_pos=url.indexOf("password=");
  // get global wifi ssid
  ssid = url.substring(s_pos+5, p_pos-1);  //accounting for the separator '&' char
  Serial.print("New wifi ssid: "); Serial.println(ssid);
  // get global wifi password
  password = url.substring(p_pos+9, url.indexOf(" HTTP"));
  Serial.print("New wifi password: "); Serial.println(password);

  if (ssid.length()>=2 && password.length()>=2)
  {
    // connecting to the new wifi
    int wifi_status = WiFi.status();
    Serial.print("WL_DISCONNECTED: "); Serial.print(WL_DISCONNECTED); Serial.print("current wifi status: ");Serial.println(wifi_status);
    Serial.println("Disconnecting from the current Wifi...");
    delay(500);
    client.stop();
    WiFi.disconnect();
    int attempt_count = 0;
    while(wifi_status != WL_DISCONNECTED && attempt_count < 10)
    {
      attempt_count  += 1;
      delay(1000);
      Serial.print(".");
      wifi_status = WiFi.status();
      Serial.print("WL_DISCONNECTED: "); Serial.print(WL_DISCONNECTED); Serial.print("; Current wifi status: ");Serial.println(wifi_status);
    }
    Serial.println("Trying to connect to Wifi...");
    attempt_count = 0;
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED && attempt_count < 10) {
      attempt_count += 1;
      delay(500);
      Serial.print(".");
    }
    // if the new credencials work, then write to file.
    if(WiFi.status() == WL_CONNECTED)
    {
      //write the new wifi to file
      Serial.print("Writing the Wifi access information to file... Connected to WiFi: "); Serial.println(WiFi.SSID());
      if(File file = SPIFFS.open("credentials", "w"))
      {
        file.println(ssid);
        file.println(password);
        file.close();
      }
    }
    
    Serial.println("Trying to connect to Wifi... 2nd time");
    attempt_count = 0;
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED && attempt_count < 10) {
      attempt_count += 1;
      delay(500);
      Serial.print(".");
    }


  
    // Print local IP address and start web server
    Serial.println("");
    Serial.print("Connected to WiFi: ");Serial.println(WiFi.SSID()); 
    Serial.print("IP address: "); Serial.println(WiFi.localIP());
    //server.begin();
  }
}

void open_gate(int gate)
{
  Serial.print("RF code OTA: ");Serial.println(gate);
  mySwitch.send(gate, 24);
}


void trigger_shade(int const *shade, int action)
{
  Serial.print("RF code OTA: ");Serial.println(shade[action]);
  mySwitch.send(shade[action], 24);
  delay(2);
  mySwitch.send(shade[action], 24);
  if(action == STOP_ACTION)
  {
    actionStartTime = 0;
  }
}


void stop_all_shades()
{
//  Serial.println("KITCHEN01 STOPPING...");
//  trigger_shade(KITCHEN01, STOP_ACTION);
//  Serial.println("KITCHEN02 STOPPING...");
//  trigger_shade(KITCHEN02, STOP_ACTION);
//  Serial.println("ATELIER   STOPPING...");
//  trigger_shade(ATELIER  , STOP_ACTION);
//  Serial.println("LIVING01  STOPPING...");
//  trigger_shade(LIVING01 , STOP_ACTION);
//  Serial.println("LIVING02  STOPPING...");
//  trigger_shade(LIVING02 , STOP_ACTION);
//  Serial.println("LIVING03  STOPPING...");
//  trigger_shade(LIVING03 , STOP_ACTION);
//  Serial.println("SUITE01   STOPPING...");
//  trigger_shade(SUITE01  , STOP_ACTION);
//  Serial.println("SUITE02   STOPPING...");
//  trigger_shade(SUITE02  , STOP_ACTION);
//  Serial.println("OFFICE    STOPPING...");
//  trigger_shade(OFFICE   , STOP_ACTION);
  Serial.println("ROOM_TI   STOPPING...");
  trigger_shade(ROOM_TI, STOP_ACTION);
//  Serial.println("WC        STOPPING...");
//  trigger_shade(WC       , STOP_ACTION);
//  Serial.println("ROOM_GUGA STOPPING...");
//  trigger_shade(ROOM_GUGA, STOP_ACTION);
}
