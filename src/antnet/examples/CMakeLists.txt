build_lib_example(
  NAME antnet-simple-example
  SOURCE_FILES antnet-simple-example.cc
  LIBRARIES_TO_LINK
    ${libantnet}
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
    ${libinternet}
    ${libapplications}
    ${libpoint-to-point}
    ${libmobility}
)

build_lib_example(
  NAME antnet-simple-wifi-example
  SOURCE_FILES antnet-simple-wifi-example.cc
  LIBRARIES_TO_LINK
    ${libantnet}
    ${libwifi}
    ${libinternet}
    ${libapplications}
    ${libpoint-to-point}
    ${libmobility}
    ${libcsma}
)

build_lib_example(
  NAME antnet-paper-example-for-track-pheromone
  SOURCE_FILES antnet-paper-example_for_track_pheromone.cc
  LIBRARIES_TO_LINK
    ${libantnet}
    ${libinternet}
    ${libapplications}
    ${libpoint-to-point}
    ${libmobility}
)