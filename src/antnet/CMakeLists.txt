set(source_files
  helper/ipv4-antnet-routing-helper.cc
  model/ant-header.cc
  model/ipv4-antnet-routing-table-entry.cc
  model/ipv4-antnet-local-traffic-statistics-entry.cc
  model/ipv4-antnet-routing.cc
)

set(header_files
  helper/ipv4-antnet-routing-helper.h
  model/ant-header.h
  model/ipv4-antnet-routing-table-entry.h
  model/ipv4-antnet-local-traffic-statistics-entry.h
  model/ipv4-antnet-routing.h
)

build_lib(
  LIBNAME antnet
  SOURCE_FILES ${source_files}
  HEADER_FILES ${header_files}
  LIBRARIES_TO_LINK
    ${libinternet}
    ${libnetwork}
    ${libcore}
    ${libapplications}
    ${libwifi}
)