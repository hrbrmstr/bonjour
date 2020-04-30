
[![Project Status: Active – The project has reached a stable, usable
state and is being actively
developed.](https://www.repostatus.org/badges/latest/active.svg)](https://www.repostatus.org/#active)
[![Signed
by](https://img.shields.io/badge/Keybase-Verified-brightgreen.svg)](https://keybase.io/hrbrmstr)
![Signed commit
%](https://img.shields.io/badge/Signed_Commits-100%25-lightgrey.svg)
[![Linux build
Status](https://travis-ci.org/hrbrmstr/bonjour.svg?branch=master)](https://travis-ci.org/hrbrmstr/bonjour)  
![Minimal R
Version](https://img.shields.io/badge/R%3E%3D-3.2.0-blue.svg)
![License](https://img.shields.io/badge/License-MIT-blue.svg)

# bonjour

Discover and Query Multicast DNS (mDNS)/zeroconf Services

## Description

Multicast DNS (mDNS) provides the ability to perform DNS-like operations
on the local link in the absence of any conventional Unicast DNS server.
Given a type of service that a client is looking for, and a domain in
which the client is looking for that service, this mechanism allows
clients to discover a list of named instances of that desired service,
using standard DNS queries. This mechanism is referred to as DNS-based
Service Discovery, or DNS-SD. Tools are provided to perform service
discovery and query for specific services over multicast DNS (mDNS).

## What’s Inside The Tin

The following functions are implemented:

  - `bnjr_discover`: Browse available services
  - `bnjr_query`: Look for a particular service

## Installation

``` r
install.packages("bonjour", repos = c("https://cinc.rud.is", "https://cloud.r-project.org/"))
# or
remotes::install_git("https://git.rud.is/hrbrmstr/bonjour.git")
# or
remotes::install_git("https://git.sr.ht/~hrbrmstr/bonjour")
# or
remotes::install_gitlab("hrbrmstr/bonjour")
# or
remotes::install_bitbucket("hrbrmstr/bonjour")
# or
remotes::install_github("hrbrmstr/bonjour")
```

NOTE: To use the ‘remotes’ install options you will need to have the
[{remotes} package](https://github.com/r-lib/remotes) installed.

## Usage

``` r
library(bonjour)
library(tidyverse)

# current version
packageVersion("bonjour")
## [1] '0.1.0'
```

``` r
bnjr_discover()
## # A tibble: 49 x 7
##    entry_type from             length name                        rclass   ttl type 
##    <chr>      <chr>             <dbl> <chr>                        <dbl> <dbl> <chr>
##  1 answer     10.1.10.59:5353      12 _ssh._tcp.local.                 1    10 PTR  
##  2 answer     10.1.10.59:5353      12 _sftp-ssh._tcp.local.            1    10 PTR  
##  3 answer     10.1.10.59:5353       8 _eppc._tcp.local.                1    10 PTR  
##  4 answer     10.1.10.59:5353       7 _rfb._tcp.local.                 1    10 PTR  
##  5 answer     10.1.10.59:5353       7 _smb._tcp.local.                 1    10 PTR  
##  6 answer     10.1.10.59:5353      17 _net-assistant._udp.local.       1    10 PTR  
##  7 answer     10.1.10.59:5353      18 _companion-link._tcp.local.      1    10 PTR  
##  8 answer     10.1.10.192:5353     12 _smb._tcp.local.                 1    10 PTR  
##  9 answer     10.1.10.20:5353      14 _adisk._tcp.local.               1    10 PTR  
## 10 answer     10.1.10.20:5353       7 _ssh._tcp.local.                 1    10 PTR  
## # … with 39 more rows
```

``` r
bnjr_query("_ssh._tcp.local.") %>% 
  filter(type == "AAAA") %>% 
  select(addr)
## # A tibble: 9 x 1
##   addr                                   
##   <chr>                                  
## 1 fe80::1410:647:b4c1:df43               
## 2 2603:3005:146a:8000:1459:ebc3:42dc:5ae2
## 3 fe80::1864:a1e2:98b:44d0               
## 4 2603:3005:146a:8000:189b:85a2:64f5:598f
## 5 fe80::18f8:a74b:6a0d:9175              
## 6 2603:3005:146a:8000:c98:42f7:9943:4b73 
## 7 2603:3005:146a:8000:6972:7ff9:9594:5467
## 8 fe80::1041:d138:41f8:efd5              
## 9 2603:3005:146a:8000:c95:dcb9:cc3a:d030
```

## bonjour Metrics

| Lang         | \# Files |  (%) | LoC |  (%) | Blank lines |  (%) | \# Lines |  (%) |
| :----------- | -------: | ---: | --: | ---: | ----------: | ---: | -------: | ---: |
| C/C++ Header |        1 | 0.11 | 936 | 0.76 |         154 | 0.55 |       87 | 0.47 |
| C++          |        2 | 0.22 | 255 | 0.21 |          97 | 0.35 |       28 | 0.15 |
| R            |        5 | 0.56 |  22 | 0.02 |          11 | 0.04 |       38 | 0.21 |
| Rmd          |        1 | 0.11 |  13 | 0.01 |          17 | 0.06 |       32 | 0.17 |

## Code of Conduct

Please note that this project is released with a Contributor Code of
Conduct. By participating in this project you agree to abide by its
terms.
