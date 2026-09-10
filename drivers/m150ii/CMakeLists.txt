#  declares the current directory as z zephyr linary

zephyr_library()

# add nets deirectory if config is set
add_subdirectory_ifdef(CONFIG_M150II_PRINTER_CHARACTER print_character)

# list thr sourece file
zephyr_library_sources(m150ii.c)

# get include dir
zephyr_include_directories(.)
