include_directories(${DEPS_INCLUDE_DIR})

set(DEP_LIBS
  stdc++
  )

add_library(decoder_main
  protobuf.cc
  main.cc
  )

add_executable(decoder
  main.cc
  )

target_link_libraries(decoder
  decoder_main
  )
