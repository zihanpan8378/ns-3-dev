build_lib_example(
  NAME antnet-wifi-adhoc
  SOURCE_FILES antnet-wifi-adhoc.cc
  LIBRARIES_TO_LINK
    ${libantnet}
    ${libwifi}
    ${libinternet}
    ${libapplications}
    ${libmobility}
)

build_lib_example(
  NAME antnet-csma-chain
  SOURCE_FILES antnet-csma-chain.cc
  LIBRARIES_TO_LINK
    ${libantnet}
    ${libinternet}
    ${libcsma}
    ${libapplications}
    ${libnetwork}
    ${libcore}
)

build_lib_example(
  NAME antnet-csma-grid
  SOURCE_FILES antnet-csma-grid.cc
  LIBRARIES_TO_LINK
    ${libantnet}
    ${libinternet}
    ${libcsma}
    ${libapplications}
    ${libnetwork}
    ${libcore}
)