#' Discover and Query Multicast DNS (mDNS)/zeroconf Services
#'
#' #' Multicast DNS (mDNS) provides the ability to perform DNS-like
#' operations on the local link in the absence of any conventional
#' Unicast DNS server. Given a type of service
#' that a client is looking for, and a domain in which the client is
#' looking for that service, this mechanism allows clients to discover
#' a list of named instances of that desired service, using standard
#' DNS queries.  This mechanism is referred to as DNS-based Service
#' Discovery, or DNS-SD. Tools are provided to perform service discovery
#' and query for specific services over multicast DNS (mDNS).
#'
#' @md
#' @name bonjour
#' @keywords internal
#' @author Bob Rudis (bob@@rud.is)
#' @importFrom jsonlite stream_in
## usethis namespace: start
#' @importFrom Rcpp sourceCpp
#' @useDynLib bonjour, .registration = TRUE
## usethis namespace: end
"_PACKAGE"
