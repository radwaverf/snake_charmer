FIND_PATH(
    snake_charmer_INCLUDE_DIRS
    NAMES snake_charmer/ring_buffer.h
    HINTS $ENV{snake_charmer_DIR}/include
    PATHS /usr/local/include
        /usr/include
        ${CMAKE_INSTALL_PREFIX}/include
)

FIND_LIBRARY(
    snake_charmer_LIBRARIES
    NAMES snake_charmer
    HINTS $ENV{snake_charmer_DIR}/lib
    PATHS /usr/local/lib
        /usr/lib
        ${CMAKE_INSTALL_PREFIX}/lib
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
    snake_charmer
    DEFAULT_MSG 
    snake_charmer_LIBRARIES
    snake_charmer_INCLUDE_DIRS
)
MARK_AS_ADVANCED(
    snake_charmer_LIBRARIES
    snake_charmer_INCLUDE_DIRS
)
