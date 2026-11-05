## =========================================================================
## @author Leonardo Florez-Valencia (florez-l@javeriana.edu.co)
## =========================================================================

# Source code
set(_name Plugin_pujOgreDotPPSceneCodec)
set(_hdr DotPPSceneLoader.h ParametricSurfaceBuilder.h)
set(_src DotPPSceneLoader.cxx ParametricSurfaceBuilder.cxx)

# Build library
include_directories(
  ${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/plugins/DotPPScene
  ${PROJECT_BINARY_DIR}/lib ${PROJECT_SOURCE_DIR}/lib
  )
add_library(${_name} SHARED ${_src})
generate_export_header(
  ${_name}
  EXPORT_MACRO_NAME ${_name}_EXPORT
  EXPORT_FILE_NAME Export.h
  )
target_link_libraries(${_name} PRIVATE pugixml pujOgre OgreTerrain)
set_target_properties(
  ${_name} PROPERTIES
  PREFIX ""
  CXX_VISIBILITY_PRESET hidden
  )
vtk_module_autoinit(TARGETS ${_name} MODULES ${VTK_LIBRARIES})

## eof - CMakeLists.txt
