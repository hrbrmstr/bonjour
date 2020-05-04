#' Browse available services
#'
#' @param scan_time how long to scan for services; default is 10 and
#'        should not really be that much lower in most networks.
#' @param proto "`ipv4`" or "`ipv6`"
#' @return data frame
#' @export
bnjr_discover <- function(scan_time = 10L, proto = c("ipv4", "ipv6")) {

  proto <- match.arg(tolower(trimws(proto[1])), c("ipv4", "ipv6"))

  res <- int_bnjr_discover(scan_time, proto = proto)
  res <- unlist(strsplit(res, "\n"))
  ndjson::flatten(res, "tbl")

}

#' @rdname bnjr_discover
#' @export
bjr_discover <- bnjr_discover

#' @rdname bnjr_discover
#' @export
mdns_discover <- bnjr_discover