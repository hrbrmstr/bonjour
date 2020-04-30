#' Look for a particular service
#'
#' @param query service to look for
#' @param scan_time how long to scan for services; default is 10 and
#'        should not really be that much lower in most networks.
#' @export
bnjr_query <- function(query, scan_time = 10L) {

  res <- int_bnjr_query(query, scan_time)
  res <- unlist(strsplit(res, "\n"))
  ndjson::flatten(res, "tbl")

}

#' @rdname bnjr_discover
#' @export
bjr_query <- bnjr_query