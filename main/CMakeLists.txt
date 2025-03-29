file(GLOB LIB_SRCS
    "../managed_components/s00500_ESPUI/src/*.cpp"
)


idf_component_register(
    SRCS 
    ${LIB_SRCS}
    "main.cpp"
    INCLUDE_DIRS "."
    "../managed_components/s00500_ESPUI/src"
)
