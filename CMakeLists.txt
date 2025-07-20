set(srcs)

list(APPEND srcs "src/Strings/ROString.cpp")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                      )

