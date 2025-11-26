build_lib_example(
  NAME p2p-examples
  SOURCE_FILES p2p-examples.cc
  LIBRARIES_TO_LINK
    ${libantnet}
    ${libinternet}
    ${libcsma}
    ${libapplications}
    ${libflow-monitor}
    ${libnetwork}
    ${libcore}
    ${libpoint-to-point}
)
