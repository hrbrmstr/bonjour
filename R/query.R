#' Look for a particular service
#'
#' @param query service to look for
#' @param scan_time how long to scan for services; default is 10 and
#'        should not really be that much lower in most networks.
#' @return data frame
#' @export
bnjr_query <- function(query, scan_time = 10L) {

  res <- int_bnjr_query(query, scan_time)
  res <- jsonlite::stream_in(textConnection(res), verbose = FALSE)
  class(res) <- c("tbl_df", "tbl", "data.frame")
  res

}

#' @rdname bnjr_query
#' @export
bjr_query <- bnjr_query

#' @rdname bnjr_query
#' @export
mdns_query <- bnjr_query