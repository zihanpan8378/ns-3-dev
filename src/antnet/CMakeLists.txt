# src/antnet/CMakeLists.txt

# 库源码
set(source_files
  model/ant-header.cc
  model/ipv4-antnet-routing-table-entry.cc
  )

# 头文件
set(header_files
  model/ant-header.h
  model/ipv4-antnet-routing-table-entry.h
)

# 生成 libantnet
build_lib(
  LIBNAME antnet
  SOURCE_FILES ${source_files}
  HEADER_FILES ${header_files}
  LIBRARIES_TO_LINK
    ${libinternet}
    ${libnetwork}
    ${libcore}
)

# ---- AntNet Examples ----

# p2p-examples 可执行文件
# set(example_source_files
#   examples/p2p-examples.cc
# )

# build_exec(
#   EXECNAME p2p-examples
#   SOURCE_FILES
#     ${example_source_files}
#   LIBRARIES_TO_LINK
#     ${libantnet}
#     ${libinternet}
#     ${libnetwork}
#     ${libcore}
#     ${libpoint-to-point}
# )

# set_target_properties(p2p-examples PROPERTIES
#   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
# )