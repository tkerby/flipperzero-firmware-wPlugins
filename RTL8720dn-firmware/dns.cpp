#include "dns.h"
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
static struct udp_pcb *dns_server_pcb;

void unbind_all_udp(){
  struct udp_pcb *pcb;
  for (pcb = udp_pcbs; pcb != NULL; pcb = pcb->next) {
      udp_remove(pcb);
  }
}

void unbind_dns(){
  //Buscamos el puerto udp de las dns para matarloo
  struct udp_pcb *pcb;
  for (pcb = udp_pcbs; pcb != NULL; pcb = pcb->next) {
    
    if(pcb->local_port==DNS_SERVER_PORT){
      udp_remove(pcb);
    }
  }
}

void start_DNS_Server(){
  dns_server_pcb = udp_new();
  udp_bind(dns_server_pcb, IP4_ADDR_ANY, DNS_SERVER_PORT);
  udp_recv(dns_server_pcb, (udp_recv_fn)dnss_receive_udp_packet_handler, NULL);
}