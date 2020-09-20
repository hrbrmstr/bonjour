#' Browse available services
#'
#' @param scan_time how long to scan for services; default is 10 and
#'        should not really be that much lower in most networks.
#' @return data frame
#' @export
bnjr_discover <- function(scan_time = 10L) {

  res <- int_bnjr_discover(scan_time)
  # res <- unlist(strsplit(res, "\n", useBytes = TRUE))
  # ndjson::flatten(res, "tbl")
  res <- jsonlite::stream_in(textConnection(res), verbose = FALSE)
  class(res) <- c("tbl_df", "tbl", "data.frame")
  res

}

#' @rdname bnjr_discover
#' @export
bjr_discover <- bnjr_discover

#' @rdname bnjr_discover
#' @export
mdns_discover <- bnjr_discover