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
#endif

static char addrbuffer[64];
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
  ipv4_address_to_string(char* buffer, size_t capacity, const struct sockaddr_in* addr, size_t addrlen) {
    char host[NI_MAXHOST] = {0};
    char service[NI_MAXSERV] = {0};
    int ret = getnameinfo((const struct sockaddr*)addr, addrlen,
                          host, NI_MAXHOST, service, NI_MAXSERV,
                          NI_NUMERICSERV | NI_NUMERICHOST);
    int len = 0;
    if (ret == 0) {
      if (addr->sin_port != 0)
        len = snprintf(buffer, capacity, "%s:%s", host, service);
      else
        len = snprintf(buffer, capacity, "%s", host);
    }
    if (len >= (int)capacity) len = (int)capacity - 1;
    mdns_string_t str = {buffer, (size_t)len};
    return str;
  }

static mdns_string_t
  ipv6_address_to_string(char* buffer, size_t capacity, const struct sockaddr_in6* addr, size_t addrlen) {
    char host[NI_MAXHOST] = {0};
    char service[NI_MAXSERV] = {0};
    int ret = getnameinfo((const struct sockaddr*)addr, addrlen,
                          host, NI_MAXHOST, service, NI_MAXSERV,
                          NI_NUMERICSERV | NI_NUMERICHOST);
    int len = 0;
    if (ret == 0) {
      if (addr->sin6_port != 0)
        len = snprintf(buffer, capacity, "[%s]:%s", host, service);
      else
        len = snprintf(buffer, capacity, "%s", host);
    }
    if (len >= (int)capacity)
      len = (int)capacity - 1;
    mdns_string_t str = {buffer, (size_t)len};
    return str;
  }

static mdns_string_t
  ip_address_to_string(char* buffer, size_t capacity, const struct sockaddr* addr, size_t addrlen) {
    if (addr->sa_family == AF_INET6)
      return ipv6_address_to_string(buffer, capacity, (const struct sockaddr_in6*)addr, addrlen);
    return ipv4_address_to_string(buffer, capacity, (const struct sockaddr_in*)addr, addrlen);
  }

static int query_callback(int sock, const struct sockaddr* from, size_t addrlen,
                          mdns_entry_type_t entry, uint16_t transaction_id,
                          uint16_t rtype, uint16_t rclass, uint32_t ttl,
                          const void* data, size_t size, size_t offset, size_t length,
                          void* user_data) {

  mdns_string_t fromaddrstr = ip_address_to_string(addrbuffer, sizeof(addrbuffer), from, addrlen);
  std::string fromstr = std::string(fromaddrstr.str, fromaddrstr.length);

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
std::string int_bnjr_discover(int scan_time = 10L, std::string proto = "ipv4") {

#ifdef _WIN32
  WORD versionWanted = MAKEWORD(1, 1);
  WSADATA wsaData;
  if (WSAStartup(versionWanted, &wsaData)) {
    Rf_warning("Failed to initialize WinSock\n");
    return();
  }
#endif

  if (!((proto == "ipv4") || (proto == "ipv6"))) proto = "ipv4";

  size_t capacity = 2048;
  void* buffer = malloc(capacity);
  //void* user_data = 0;
  size_t records;

  std::string out;

  std::FILE* tmpf = std::tmpfile();

  int port = 0;
  int sock = (proto == "ipv4") ? mdns_socket_open_ipv4(port) : mdns_socket_open_ipv6(port);

  if (sock < 0) {
    Rf_warning("Failed to open socket: %s\n", strerror(errno));
    goto quit_int_bnjr_discover;
  }

  if (mdns_discovery_send(sock)) {
    Rf_warning("Failed to send DNS-DS discovery: %s\n", strerror(errno));
    goto quit_int_bnjr_discover;
  }

  for (int i = 0; i < scan_time; ++i) {

    do {
      records = mdns_discovery_recv(
        sock, buffer, capacity, query_callback, tmpf
      );
    } while (records);

    if (records) i = 0;

    sleep(1);

  }

  std::rewind(tmpf);

  char fbuf[1024];

  while (std::fgets(fbuf, sizeof(fbuf), tmpf)) {
    out = out + std::string(fbuf, strlen(fbuf));
  }

  std::fclose(tmpf);

  quit_int_bnjr_discover:

    free(buffer);

  if (sock >= 0) mdns_socket_close(sock);

#ifdef _WIN32
  WSACleanup();
#endif

  return(out);

}

// [[Rcpp::export]]
std::string int_bnjr_query(std::string q, int scan_time = 5L, std::string proto = "ipv4") {

#ifdef _WIN32
  WORD versionWanted = MAKEWORD(1, 1);
  WSADATA wsaData;
  if (WSAStartup(versionWanted, &wsaData)) {
    Rf_warning("Failed to initialize WinSock\n");
    return();
  }
#endif

  if (!((proto == "ipv4") || (proto == "ipv6"))) proto = "ipv4";

  size_t capacity = 2048;
  void* buffer = malloc(capacity);
  //void* user_data = 0;
  size_t records;

  std::FILE* tmpf = std::tmpfile();

  std::string out;

  int port = 0;
  int sock = (proto == "ipv4") ? mdns_socket_open_ipv4(port) : mdns_socket_open_ipv6(port);

  if (sock < 0) {
    Rf_warning("Failed to open socket: %s\n", strerror(errno));
    goto quit_int_bnjr_query_send;
  }

  if (mdns_query_send(sock, MDNS_RECORDTYPE_PTR,
                      q.c_str(), q.length(),
                      buffer, capacity)) {
    Rf_warning("Failed to send mDNS query: %s\n", strerror(errno));
    goto quit_int_bnjr_query_send;
  }

  for (int i = 0; i < scan_time; ++i) {

    do {
      records = mdns_query_recv(
        sock, buffer, capacity, query_callback, tmpf, 1
      );
    } while (records);

    if (records) i = 0;

    sleep(1);

  }

  std::rewind(tmpf);

  char fbuf[1024];

  while (std::fgets(fbuf, sizeof(fbuf), tmpf)) {
    out = out + std::string(fbuf, strlen(fbuf));
  }

  std::fclose(tmpf);

  quit_int_bnjr_query_send:

    free(buffer);

  if (sock >= 0) mdns_socket_close(sock);

#ifdef _WIN32
  WSACleanup();
#endif

  return(out);

}