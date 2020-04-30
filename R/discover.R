#' Browse available services
#'
#' @param scan_time how long to scan for services; default is 10 and
#'        should not really be that much lower in most networks.
#' @export
bnjr_discover <- function(scan_time = 10L) {

  res <- int_bnjr_discover(scan_time)
  res <- unlist(strsplit(res, "\n"))
  ndjson::flatten(res, "tbl")

}

#' @rdname bnjr_discover
#' @export
bjr_discover <- bnjr_discover