build_lib_example(
  NAME antnet-simple-example
  SOURCE_FILES antnet-simple-example.cc
  LIBRARIES_TO_LINK
    ${libantnet}
    ${libwifi}
    ${libinternet}
    ${libapplications}
    ${libpoint-to-point}
    ${libmobility}
)

build_lib_example(
  NAME antnet-paper-example
  SOURCE_FILES antnet-paper-example.cc
  LIBRARIES_TO_LINK
    ${libantnet}
    ${libwifi}
    ${libinternet}
    ${libapplications}
    ${libpoint-to-point}
    ${libmobility}
)
