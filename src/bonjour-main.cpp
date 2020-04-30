#include <Rcpp.h>
#include <stdio.h>
#include <errno.h>
#include "mdns.h"

using namespace Rcpp;

#ifdef _WIN32
#  include <iphlpapi.h>
#  define sleep(x) Sleep(x * 1000)
#else
#  include <netdb.h>
#endif

static char addrbuffer[64];
static char namebuffer[256];
static char sendbuffer[256];
static mdns_record_txt_t txtbuffer[128];

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
    if (len >= (int)capacity)
      len = (int)capacity - 1;
    mdns_string_t str = {buffer, len};
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
    mdns_string_t str = {buffer, len};
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

  List *l = static_cast<List *>(user_data);

  mdns_string_t fromaddrstr = ip_address_to_string(addrbuffer, sizeof(addrbuffer), from, addrlen);

  const char* entrytype = (entry == MDNS_ENTRYTYPE_ANSWER) ? "answer" :
    ((entry == MDNS_ENTRYTYPE_AUTHORITY) ? "authority" : "additional");

  if (rtype == MDNS_RECORDTYPE_PTR) {

    mdns_string_t namestr = mdns_record_parse_ptr(data, size, offset, length,
                                                  namebuffer, sizeof(namebuffer));

    List ptr_l = List::create(
      _["from"]   = String(fromaddrstr.str),
      _["name"]   = String(namestr.str),
      _["type"]   = String("PTR"),
      _["entry_type"] = String(entrytype),
      _["rclass"] = rclass,
      _["ttl"]    = ttl,
      _["length"] = length
    );

    l->push_back(ptr_l);

    printf("%.*s : %s PTR %.*s rclass 0x%x ttl %u length %d\n",
           MDNS_STRING_FORMAT(fromaddrstr), entrytype,
           MDNS_STRING_FORMAT(namestr), rclass, ttl, (int)length);

  } else if (rtype == MDNS_RECORDTYPE_SRV) {

    mdns_record_srv_t srv = mdns_record_parse_srv(data, size, offset, length,
                                                  namebuffer, sizeof(namebuffer));

    List srv_l = List::create(
      _["from"]     = String(fromaddrstr.str),
      _["type"]     = String("SRV"),
      _["entry_type"] = String(entrytype),
      _["priority"] = srv.priority,
      _["weight"]   = srv.weight,
      _["port"]     = srv.port
    );

    l->push_back(srv_l);

    printf("%.*s : %s SRV %.*s priority %d weight %d port %d\n",
           MDNS_STRING_FORMAT(fromaddrstr), entrytype,
           MDNS_STRING_FORMAT(srv.name), srv.priority, srv.weight, srv.port);

  } else if (rtype == MDNS_RECORDTYPE_A) {

    struct sockaddr_in addr;
    mdns_record_parse_a(data, size, offset, length, &addr);
    mdns_string_t addrstr = ipv4_address_to_string(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));

    List a_l = List::create(
      _["from"] = String(fromaddrstr.str),
      _["type"] = String("A"),
      _["entry_type"] = String(entrytype),
      _["addr"] = String(addrstr.str)
    );

    l->push_back(a_l);

    printf("%.*s : %s A %.*s\n",
           MDNS_STRING_FORMAT(fromaddrstr), entrytype,
           MDNS_STRING_FORMAT(addrstr));

  } else if (rtype == MDNS_RECORDTYPE_AAAA) {

    struct sockaddr_in6 addr;
    mdns_record_parse_aaaa(data, size, offset, length, &addr);
    mdns_string_t addrstr = ipv6_address_to_string(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));

    List aaaa_l = List::create(
      _["from"] = String(fromaddrstr.str),
      _["type"] = String("AAAA"),
      _["entry_type"] = String(entrytype),
      _["addr"] = String(addrstr.str)
    );

    l->push_back(aaaa_l);

    printf("%.*s : %s AAAA %.*s\n",
           MDNS_STRING_FORMAT(fromaddrstr), entrytype,
           MDNS_STRING_FORMAT(addrstr));

  } else if (rtype == MDNS_RECORDTYPE_TXT) {

    size_t parsed = mdns_record_parse_txt(data, size, offset, length,
                                          txtbuffer, sizeof(txtbuffer) / sizeof(mdns_record_txt_t));
    for (size_t itxt = 0; itxt < parsed; ++itxt) {

      if (txtbuffer[itxt].value.length) {

        printf("%.*s : %s TXT %.*s = %.*s\n",
               MDNS_STRING_FORMAT(fromaddrstr), entrytype,
               MDNS_STRING_FORMAT(txtbuffer[itxt].key),
               MDNS_STRING_FORMAT(txtbuffer[itxt].value));

        List txt_l = List::create(
          _["from"] = String(fromaddrstr.str),
          _["type"] = String("TXT"),
          _["entry_type"] = String(entrytype),
          _["key"] = String(txtbuffer[itxt].key.str),
          _["value"] = String(txtbuffer[itxt].value.str)
        );

        l->push_back(txt_l);

      } else {

        printf("%.*s : %s TXT %.*s\n",
               MDNS_STRING_FORMAT(fromaddrstr), entrytype,
               MDNS_STRING_FORMAT(txtbuffer[itxt].key));

        List txt_l = List::create(
          _["from"] = String(fromaddrstr.str),
          _["type"] = String("TXT"),
          _["entry_type"] = String(entrytype),
          _["key"] = String(txtbuffer[itxt].key.str)
        );

        l->push_back(txt_l);

      }

    }

  } else {

    List generic_l = List::create(
      _["from"]   = String(fromaddrstr.str),
      _["entry_type"] = entrytype,
      _["type"]   = NA_STRING,
      _["rclass"] = rclass,
      _["ttl"]    = ttl,
      _["length"] = length
    );

    l->push_back(generic_l);

    printf("%.*s : %s type %u rclass 0x%x ttl %u length %d\n",
           MDNS_STRING_FORMAT(fromaddrstr), entrytype,
           rtype, rclass, ttl, (int)length);

  }

  return 0;

}

//' @export
// [[Rcpp::export]]
List int_bnjr_discover() {

#ifdef _WIN32
  WORD versionWanted = MAKEWORD(1, 1);
  WSADATA wsaData;
  if (WSAStartup(versionWanted, &wsaData)) {
    printf("Failed to initialize WinSock\n");
    return -1;
  }
#endif

  size_t capacity = 2048;
  void* buffer = malloc(capacity);
  size_t records;

  List out = List::create();

  int port = 0;
  int sock = mdns_socket_open_ipv4(port);

  if (sock < 0) return(out);

  if (mdns_discovery_send(sock)) {
    goto failnice;
  }

  for (int i = 0; i < 10; ++i) {

    do {

      records = mdns_discovery_recv(
        sock, buffer, capacity,
        query_callback, &out
      );

    } while (records);

    if (records) i = 0;

    sleep(1);

  }

  failnice:
    free(buffer);

    mdns_socket_close(sock);

#ifdef _WIN32
    WSACleanup();
#endif

    return(out);

}

//' @export
// [[Rcpp::export]]
List int_bnjr_query_send(std::string svc) {

#ifdef _WIN32
  WORD versionWanted = MAKEWORD(1, 1);
  WSADATA wsaData;
  if (WSAStartup(versionWanted, &wsaData)) {
    printf("Failed to initialize WinSock\n");
    return -1;
  }
#endif

  size_t capacity = 2048;
  void* buffer = malloc(capacity);
  void* user_data = 0;
  size_t records;

  List out = List::create();

  int port = 0;
  int sock = mdns_socket_open_ipv4(port);

  if (sock < 0) return(out);

  Rcout << "SERVICE: " << svc.c_str() << std::endl;

  if (mdns_discovery_send(sock)) {
    Rcout << "FAILED" << std::endl;
    goto failnice2;
  }

  if (mdns_query_send(sock, MDNS_RECORDTYPE_PTR,
                      svc.c_str(), svc.length(),
                  buffer, capacity)) {
    Rcout << "FAILED" << std::endl;
    goto failnice2;
  }

  for (int i = 0; i < 10; ++i) {

    do {
      records = mdns_query_recv(
        sock, buffer, capacity, query_callback, &out, 1
      );

      Rcout << "RECORDS: " << records << std::endl;

    } while (records);

    if (records) i = 0;

    sleep(1);

  }

  failnice2:
    free(buffer);

    mdns_socket_close(sock);

#ifdef _WIN32
    WSACleanup();
#endif

   return(out);

}
