#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "hdr_histogram::hdr_histogram" for configuration "Debug"
set_property(TARGET hdr_histogram::hdr_histogram APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(hdr_histogram::hdr_histogram PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libhdr_histogram.so.6.2.3"
  IMPORTED_SONAME_DEBUG "libhdr_histogram.so.6"
  )

list(APPEND _cmake_import_check_targets hdr_histogram::hdr_histogram )
list(APPEND _cmake_import_check_files_for_hdr_histogram::hdr_histogram "${_IMPORT_PREFIX}/lib/libhdr_histogram.so.6.2.3" )

# Import target "hdr_histogram::hdr_histogram_static" for configuration "Debug"
set_property(TARGET hdr_histogram::hdr_histogram_static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(hdr_histogram::hdr_histogram_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libhdr_histogram_static.a"
  )

list(APPEND _cmake_import_check_targets hdr_histogram::hdr_histogram_static )
list(APPEND _cmake_import_check_files_for_hdr_histogram::hdr_histogram_static "${_IMPORT_PREFIX}/lib/libhdr_histogram_static.a" )

# Import target "hdr_histogram::hdr_decoder" for configuration "Debug"
set_property(TARGET hdr_histogram::hdr_decoder APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(hdr_histogram::hdr_decoder PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/hdr_decoder"
  )

list(APPEND _cmake_import_check_targets hdr_histogram::hdr_decoder )
list(APPEND _cmake_import_check_files_for_hdr_histogram::hdr_decoder "${_IMPORT_PREFIX}/bin/hdr_decoder" )

# Import target "hdr_histogram::hiccup" for configuration "Debug"
set_property(TARGET hdr_histogram::hiccup APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(hdr_histogram::hiccup PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/hiccup"
  )

list(APPEND _cmake_import_check_targets hdr_histogram::hiccup )
list(APPEND _cmake_import_check_files_for_hdr_histogram::hiccup "${_IMPORT_PREFIX}/bin/hiccup" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
