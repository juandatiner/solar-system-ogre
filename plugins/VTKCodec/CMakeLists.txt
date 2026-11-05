## =========================================================================
## @author Leonardo Florez-Valencia (florez-l@javeriana.edu.co)
## =========================================================================

# Source code
set(_name Plugin_pujOgreVTKCodec)
set(_hdr)
set(_src OgreVTKCodec.cxx)

# Build library
include_directories(
  ${PROJECT_BINARY_DIR}
  ${PROJECT_BINARY_DIR}/lib ${PROJECT_SOURCE_DIR}/lib
  )
add_library(${_name} SHARED ${_src})
generate_export_header(
  ${_name}
  EXPORT_MACRO_NAME ${_name}_EXPORT
  EXPORT_FILE_NAME Export.h
  )
target_link_libraries(${_name} PRIVATE pujOgre)
set_target_properties(
  ${_name} PROPERTIES
  PREFIX ""
  CXX_VISIBILITY_PRESET hidden
  )
vtk_module_autoinit(TARGETS ${_name} MODULES ${VTK_LIBRARIES})

## eof - CMakeLists.txt
