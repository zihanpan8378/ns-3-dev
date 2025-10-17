set(source_files
  helper/antnet-helper.cc
  model/antnet-routing-protocol.cc
  model/ant-headers.cc
  model/pheromone-table.cc
)

set(header_files
  helper/antnet-helper.h
  model/antnet-routing-protocol.h
  model/ant-headers.h
  model/pheromone-table.h
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
