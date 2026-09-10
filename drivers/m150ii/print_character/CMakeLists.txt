add_subdirectory(./font)

# list thr sourece file
zephyr_library_sources(m150ii-printer.c)

# get include dir
zephyr_include_directories(.)
