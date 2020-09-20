#include <Rcpp.h>
#include <cstdio>
#include <fstream>
#include <iostream>

using namespace Rcpp;

#include "mdns.h"

#include <stdio.h>
#include <errno.h>

#ifdef _WIN32
#  include <iphlpapi.h>
#  define sleep(x) Sleep(x * 1000)
#else
#  include <netdb.h>
#  include <ifaddrs.h>
#endif

static uint32_t service_address_ipv4;
static uint8_t service_address_ipv6[16];

static int has_ipv4;
static int has_ipv6;

static char addrbuffer[64];
static char entrybuffer[256];
static char namebuffer[256];
//static char sendbuffer[256];
static mdns_record_txt_t txtbuffer[128];

typedef struct {
  const char* service;
  const char* hostname;
  uint32_t address_ipv4;
  uint8_t* address_ipv6;
  int port;
} service_record_t;



static mdns_string_t
  ipv4_address_to_string(char* buffer, size_t capacity, const struct sockaddr_in* addr,
                         size_t addrlen) {
    char host[NI_MAXHOST] = {0};
    char service[NI_MAXSERV] = {0};
    int ret = getnameinfo((const struct sockaddr*)addr, (socklen_t)addrlen, host, NI_MAXHOST,
                          service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
    int len = 0;
    if (ret == 0) {
      if (addr->sin_port != 0)
        len = snprintf(buffer, capacity, "%s:%s", host, service);
      else
        len = snprintf(buffer, capacity, "%s", host);
    }
    if (len >= (int)capacity)
      len = (int)capacity - 1;
    mdns_string_t str;
    str.str = buffer;
    str.length = len;
    return str;
  }

static mdns_string_t
  ipv6_address_to_string(char* buffer, size_t capacity, const struct sockaddr_in6* addr,
                         size_t addrlen) {
    char host[NI_MAXHOST] = {0};
    char service[NI_MAXSERV] = {0};
    int ret = getnameinfo((const struct sockaddr*)addr, (socklen_t)addrlen, host, NI_MAXHOST,
                          service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
    int len = 0;
    if (ret == 0) {
      if (addr->sin6_port != 0)
        len = snprintf(buffer, capacity, "[%s]:%s", host, service);
      else
        len = snprintf(buffer, capacity, "%s", host);
    }
    if (len >= (int)capacity)
      len = (int)capacity - 1;
    mdns_string_t str;
    str.str = buffer;
    str.length = len;
    return str;
  }

static mdns_string_t
  ip_address_to_string(char* buffer, size_t capacity, const struct sockaddr* addr, size_t addrlen) {
    if (addr->sa_family == AF_INET6)
      return ipv6_address_to_string(buffer, capacity, (const struct sockaddr_in6*)addr, addrlen);
    return ipv4_address_to_string(buffer, capacity, (const struct sockaddr_in*)addr, addrlen);
  }



static int
  open_client_sockets(int* sockets, int max_sockets, int port) {
    // When sending, each socket can only send to one network interface
    // Thus we need to open one socket for each interface and address family
    int num_sockets = 0;

#ifdef _WIN32

    IP_ADAPTER_ADDRESSES* adapter_address = 0;
    ULONG address_size = 8000;
    unsigned int ret;
    unsigned int num_retries = 4;
    do {
      adapter_address = malloc(address_size);
      ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST, 0,
                                 adapter_address, &address_size);
      if (ret == ERROR_BUFFER_OVERFLOW) {
        free(adapter_address);
        adapter_address = 0;
      } else {
        break;
      }
    } while (num_retries-- > 0);

    if (!adapter_address || (ret != NO_ERROR)) {
      free(adapter_address);
      printf("Failed to get network adapter addresses\n");
      return num_sockets;
    }

    int first_ipv4 = 1;
    int first_ipv6 = 1;
    for (PIP_ADAPTER_ADDRESSES adapter = adapter_address; adapter; adapter = adapter->Next) {
      if (adapter->TunnelType == TUNNEL_TYPE_TEREDO)
        continue;
      if (adapter->OperStatus != IfOperStatusUp)
        continue;

      for (IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast;
      unicast = unicast->Next) {
        if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
          struct sockaddr_in* saddr = (struct sockaddr_in*)unicast->Address.lpSockaddr;
          if ((saddr->sin_addr.S_un.S_un_b.s_b1 != 127) ||
              (saddr->sin_addr.S_un.S_un_b.s_b2 != 0) ||
              (saddr->sin_addr.S_un.S_un_b.s_b3 != 0) ||
              (saddr->sin_addr.S_un.S_un_b.s_b4 != 1)) {
            int log_addr = 0;
            if (first_ipv4) {
              service_address_ipv4 = saddr->sin_addr.S_un.S_addr;
              first_ipv4 = 0;
              log_addr = 1;
            }
            has_ipv4 = 1;
            if (num_sockets < max_sockets) {
              saddr->sin_port = htons((unsigned short)port);
              int sock = mdns_socket_open_ipv4(saddr);
              if (sock >= 0) {
                sockets[num_sockets++] = sock;
                log_addr = 1;
              } else {
                log_addr = 0;
              }
            }
            if (log_addr) {
              char buffer[128];
              mdns_string_t addr = ipv4_address_to_string(buffer, sizeof(buffer), saddr,
                                                          sizeof(struct sockaddr_in));
              // printf("Local IPv4 address: %.*s\n", MDNS_STRING_FORMAT(addr));
            }
          }
        } else if (unicast->Address.lpSockaddr->sa_family == AF_INET6) {
          struct sockaddr_in6* saddr = (struct sockaddr_in6*)unicast->Address.lpSockaddr;
          static const unsigned char localhost[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                    0, 0, 0, 0, 0, 0, 0, 1};
          static const unsigned char localhost_mapped[] = {0, 0, 0,    0,    0,    0, 0, 0,
                                                           0, 0, 0xff, 0xff, 0x7f, 0, 0, 1};
          if ((unicast->DadState == NldsPreferred) &&
              memcmp(saddr->sin6_addr.s6_addr, localhost, 16) &&
              memcmp(saddr->sin6_addr.s6_addr, localhost_mapped, 16)) {
            int log_addr = 0;
            if (first_ipv6) {
              memcpy(service_address_ipv6, &saddr->sin6_addr, 16);
              first_ipv6 = 0;
              log_addr = 1;
            }
            has_ipv6 = 1;
            if (num_sockets < max_sockets) {
              saddr->sin6_port = htons((unsigned short)port);
              int sock = mdns_socket_open_ipv6(saddr);
              if (sock >= 0) {
                sockets[num_sockets++] = sock;
                log_addr = 1;
              } else {
                log_addr = 0;
              }
            }
            if (log_addr) {
              char buffer[128];
              mdns_string_t addr = ipv6_address_to_string(buffer, sizeof(buffer), saddr,
                                                          sizeof(struct sockaddr_in6));
              // printf("Local IPv6 address: %.*s\n", MDNS_STRING_FORMAT(addr));
            }
          }
        }
      }
    }

    free(adapter_address);

#else

    struct ifaddrs* ifaddr = 0;
    struct ifaddrs* ifa = 0;

    if (getifaddrs(&ifaddr) < 0)
      printf("Unable to get interface addresses\n");

    int first_ipv4 = 1;
    int first_ipv6 = 1;
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
      if (!ifa->ifa_addr)
        continue;

      if (ifa->ifa_addr->sa_family == AF_INET) {
        struct sockaddr_in* saddr = (struct sockaddr_in*)ifa->ifa_addr;
        if (saddr->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
          int log_addr = 0;
          if (first_ipv4) {
            service_address_ipv4 = saddr->sin_addr.s_addr;
            first_ipv4 = 0;
            log_addr = 1;
          }
          has_ipv4 = 1;
          if (num_sockets < max_sockets) {
            saddr->sin_port = htons(port);
            int sock = mdns_socket_open_ipv4(saddr);
            if (sock >= 0) {
              sockets[num_sockets++] = sock;
              log_addr = 1;
            } else {
              log_addr = 0;
            }
          }
          if (log_addr) {
            char buffer[128];
            mdns_string_t addr = ipv4_address_to_string(buffer, sizeof(buffer), saddr,
                                                        sizeof(struct sockaddr_in));
            // printf("Local IPv4 address: %.*s\n", MDNS_STRING_FORMAT(addr));
          }
        }
      } else if (ifa->ifa_addr->sa_family == AF_INET6) {
        struct sockaddr_in6* saddr = (struct sockaddr_in6*)ifa->ifa_addr;
        static const unsigned char localhost[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                  0, 0, 0, 0, 0, 0, 0, 1};
        static const unsigned char localhost_mapped[] = {0, 0, 0,    0,    0,    0, 0, 0,
                                                         0, 0, 0xff, 0xff, 0x7f, 0, 0, 1};
        if (memcmp(saddr->sin6_addr.s6_addr, localhost, 16) &&
            memcmp(saddr->sin6_addr.s6_addr, localhost_mapped, 16)) {
          int log_addr = 0;
          if (first_ipv6) {
            memcpy(service_address_ipv6, &saddr->sin6_addr, 16);
            first_ipv6 = 0;
            log_addr = 1;
          }
          has_ipv6 = 1;
          if (num_sockets < max_sockets) {
            saddr->sin6_port = htons(port);
            int sock = mdns_socket_open_ipv6(saddr);
            if (sock >= 0) {
              sockets[num_sockets++] = sock;
              log_addr = 1;
            } else {
              log_addr = 0;
            }
          }
          if (log_addr) {
            char buffer[128];
            mdns_string_t addr = ipv6_address_to_string(buffer, sizeof(buffer), saddr,
                                                        sizeof(struct sockaddr_in6));
            // printf("Local IPv6 address: %.*s\n", MDNS_STRING_FORMAT(addr));
          }
        }
      }
    }

    freeifaddrs(ifaddr);

#endif

    return num_sockets;
  }

static int query_callback(int sock,
                          const struct sockaddr* from,
                          size_t addrlen,
                          mdns_entry_type_t entry,
                          uint16_t transaction_id,
                          uint16_t rtype,
                          uint16_t rclass,
                          uint32_t ttl,
                          const void* data,
                          size_t size,
                          size_t name_offset,
                          size_t name_length,
                          size_t offset,
                          size_t length,
                          void* user_data) {

// static int query_callback(int sock, const struct sockaddr* from, size_t addrlen,
//                           mdns_entry_type_t entry, uint16_t transaction_id,
//                           uint16_t rtype, uint16_t rclass, uint32_t ttl,
//                           const void* data, size_t size, size_t offset, size_t length,
//                           void* user_data) {

  mdns_string_t fromaddrstr = ip_address_to_string(addrbuffer, sizeof(addrbuffer), from, addrlen);
  std::string fromstr = std::string(fromaddrstr.str, fromaddrstr.length);

  mdns_string_t entrystr =
    mdns_string_extract(data, size, &name_offset, entrybuffer, sizeof(entrybuffer));

  const char* entrytype = (entry == MDNS_ENTRYTYPE_ANSWER) ? "answer" :
    ((entry == MDNS_ENTRYTYPE_AUTHORITY) ? "authority" : "additional");

  std::string rec = "{ \"from\" : \"" + fromstr +
                    "\", \"entry_type\": \"" + entrytype + "\"";

  if (rtype == MDNS_RECORDTYPE_PTR) {

    mdns_string_t namestr = mdns_record_parse_ptr(data, size, offset, length,
                                                  namebuffer, sizeof(namebuffer));

    rec = rec + ", \"type\": \"PTR\"";
    rec = rec + ", \"name\": \"" + std::string(namestr.str, namestr.length) + "\"";
    rec = rec + ", \"rclass\": " + std::to_string(rclass) + "";
    rec = rec + ", \"ttl\": " + std::to_string(ttl) + "";
    rec = rec + ", \"length\": " + std::to_string(length) + "";

    // printf("%.*s : %s PTR %.*s rclass 0x%x ttl %u length %d\n",
    //        MDNS_STRING_FORMAT(fromaddrstr), entrytype,
    //        MDNS_STRING_FORMAT(namestr), rclass, ttl, (int)length);

  } else if (rtype == MDNS_RECORDTYPE_SRV) {

    mdns_record_srv_t srv = mdns_record_parse_srv(data, size, offset, length,
                                                  namebuffer, sizeof(namebuffer));

    rec = rec + ", \"type\": \"SRV\"";
    rec = rec + ", \"srv_name\": \"" + std::string(srv.name.str, srv.name.length) + "\"";
    rec = rec + ", \"srv_priority\": " + std::to_string(srv.priority) + "";
    rec = rec + ", \"srv_weight\": " + std::to_string(srv.weight) + "";
    rec = rec + ", \"srv_port\": " + std::to_string(srv.port) + "";

    // printf("%.*s : %s SRV %.*s priority %d weight %d port %d\n",
    //        MDNS_STRING_FORMAT(fromaddrstr), entrytype,
    //        MDNS_STRING_FORMAT(srv.name), srv.priority, srv.weight, srv.port);

  } else if (rtype == MDNS_RECORDTYPE_A) {

    struct sockaddr_in addr;
    mdns_record_parse_a(data, size, offset, length, &addr);
    mdns_string_t addrstr = ipv4_address_to_string(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));

    rec = rec + ", \"type\": \"A\"";
    rec = rec + ", \"addr\": \"" + std::string(addrstr.str, addrstr.length) + "\"";

    // printf("%.*s : %s A %.*s\n",
    //        MDNS_STRING_FORMAT(fromaddrstr), entrytype,
    //        MDNS_STRING_FORMAT(addrstr));

  } else if (rtype == MDNS_RECORDTYPE_AAAA) {

    struct sockaddr_in6 addr;
    mdns_record_parse_aaaa(data, size, offset, length, &addr);
    mdns_string_t addrstr = ipv6_address_to_string(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));

    rec = rec + ", \"type\": \"AAAA\"";
    rec = rec + ", \"addr\": \"" + std::string(addrstr.str, addrstr.length) + "\"";

    // printf("%.*s : %s AAAA %.*s\n",
    //        MDNS_STRING_FORMAT(fromaddrstr), entrytype,
    //        MDNS_STRING_FORMAT(addrstr));

  } else if (rtype == MDNS_RECORDTYPE_TXT) {

    size_t parsed = mdns_record_parse_txt(data, size, offset, length,
                                          txtbuffer, sizeof(txtbuffer) / sizeof(mdns_record_txt_t));

    for (size_t itxt = 0; itxt < parsed; ++itxt) {

      rec = rec + ", \"type\": \"TXT\"";

      if (txtbuffer[itxt].value.length) {

        rec = rec + ", \"key\": \"" + std::string(txtbuffer[itxt].key.str, txtbuffer[itxt].key.length) + "\"";
        rec = rec + ", \"key\": \"" + std::string(txtbuffer[itxt].value.str, txtbuffer[itxt].value.length) + "\"";

        // printf("%.*s : %s TXT %.*s = %.*s\n",
        //        MDNS_STRING_FORMAT(fromaddrstr), entrytype,
        //        MDNS_STRING_FORMAT(txtbuffer[itxt].key),
        //        MDNS_STRING_FORMAT(txtbuffer[itxt].value));

      } else {

        rec = rec + ", \"key\": \"" + std::string(txtbuffer[itxt].key.str, txtbuffer[itxt].key.length) + "\"";

        // printf("%.*s : %s TXT %.*s\n",
        //        MDNS_STRING_FORMAT(fromaddrstr), entrytype,
        //        MDNS_STRING_FORMAT(txtbuffer[itxt].key));
      }

    }

  } else {

    rec = rec + ", \"type\": \"" + std::to_string((long)entrytype) + "";
    rec = rec + ", \"rclass\": " + std::to_string(rclass) + "";
    rec = rec + ", \"rtype\": " + std::to_string(rtype) + "";
    rec = rec + ", \"ttl\": " + std::to_string(ttl) + "";
    rec = rec + ", \"length\": " + std::to_string(length) + "";

    // printf("%.*s : %s type %u rclass 0x%x ttl %u length %d\n",
    //        MDNS_STRING_FORMAT(fromaddrstr), entrytype,
    //        rtype, rclass, ttl, (int)length);

  }

  rec = rec + " }\n";

  std::fputs(rec.c_str(), (FILE *)user_data);

  return 0;

}

// [[Rcpp::export]]
std::string int_bnjr_discover(int scan_time = 10L) {

  int sockets[32];
  int num_sockets = open_client_sockets(sockets, sizeof(sockets) / sizeof(sockets[0]), 0);
  if (num_sockets <= 0) Rf_error("Failed to open any client sockets\n");

  std::string out;

  std::FILE* tmpf = std::tmpfile();

  for (int isock = 0; isock < num_sockets; ++isock) {
    if ((mdns_discovery_send(sockets[isock])) && (errno != EHOSTUNREACH))
      Rf_warning("Failed to send DNS-DS discovery: %s\n", strerror(errno));
  }

  size_t capacity = 2048;
  void* buffer = malloc(capacity);
  void* user_data = 0;
  size_t records;

  int res;

  do {
    struct timeval timeout;
    timeout.tv_sec = scan_time;
    timeout.tv_usec = 0;

    int nfds;
    nfds = 0;
    fd_set readfs;
    FD_ZERO(&readfs);
    for (int isock = 0; isock < num_sockets; ++isock) {
      if (sockets[isock] >= nfds)
        nfds = sockets[isock] + 1;
      FD_SET(sockets[isock], &readfs);
    }

    records = 0;
    res = select(nfds, &readfs, 0, 0, &timeout);
    if (res > 0) {
      for (int isock = 0; isock < num_sockets; ++isock) {
        if (FD_ISSET(sockets[isock], &readfs)) {
          records += mdns_discovery_recv(sockets[isock],
                                         buffer,
                                         capacity,
                                         query_callback,
                                         tmpf);
        }
      }
    }
  } while (res > 0);

  std::rewind(tmpf);

  char fbuf[1024];

  while (std::fgets(fbuf, sizeof(fbuf), tmpf)) {
    out = out + std::string(fbuf, strlen(fbuf));
  }

  std::fclose(tmpf);

  free(buffer);

  for (int isock = 0; isock < num_sockets; ++isock){
    mdns_socket_close(sockets[isock]);
  }

  return(out);

}

// [[Rcpp::export]]
std::string int_bnjr_query(std::string q, int scan_time = 5L) {

  int sockets[32];
  int query_id[32];

  int num_sockets = open_client_sockets(sockets, sizeof(sockets) / sizeof(sockets[0]), 0);
  if (num_sockets <= 0) Rf_error("Failed to open any client sockets");

  size_t capacity = 2048;
  void* buffer = malloc(capacity);
  void* user_data = 0;
  size_t records;

  std::FILE* tmpf = std::tmpfile();

  std::string out;

  for (int isock = 0; isock < num_sockets; ++isock) {
    query_id[isock] = mdns_query_send(sockets[isock], MDNS_RECORDTYPE_PTR, q.c_str(),
                                      q.length(), buffer, capacity, 0);
    if ((query_id[isock] < 0) && (errno != EHOSTUNREACH))
      Rf_warning("Failed to send mDNS query: %s\n", strerror(errno));
  }

  int res;

  do {
    struct timeval timeout;
    timeout.tv_sec = scan_time;
    timeout.tv_usec = 0;

    int nfds = 0;
    fd_set readfs;
    FD_ZERO(&readfs);
    for (int isock = 0; isock < num_sockets; ++isock) {
      if (sockets[isock] >= nfds)
        nfds = sockets[isock] + 1;
      FD_SET(sockets[isock], &readfs);
    }

    records = 0;
    res = select(nfds, &readfs, 0, 0, &timeout);
    if (res > 0) {
      for (int isock = 0; isock < num_sockets; ++isock) {
        if (FD_ISSET(sockets[isock], &readfs)) {
          records += mdns_query_recv(sockets[isock], buffer, capacity, query_callback,
                                     tmpf, query_id[isock]);
        }
        FD_SET(sockets[isock], &readfs);
      }
    }
  } while (res > 0);

  std::rewind(tmpf);

  char fbuf[1024];

  while (std::fgets(fbuf, sizeof(fbuf), tmpf)) {
    out = out + std::string(fbuf, strlen(fbuf));
  }

  std::fclose(tmpf);

  free(buffer);

  for (int isock = 0; isock < num_sockets; ++isock)
    mdns_socket_close(sockets[isock]);

  return(out);

}