## =========================================================================
## @author Leonardo Florez-Valencia (florez-l@javeriana.edu.co)
## =========================================================================

# Source code
set(_name pujOgre)
set(_hdr vtkParametricExpression.h vtkPolyData2OgreMesh.h)
set(_src vtkParametricExpression.cxx vtkPolyData2OgreMesh.cxx)

# Build library
include_directories(${PROJECT_BINARY_DIR}/lib ${PROJECT_SOURCE_DIR}/lib)
add_library(${_name} SHARED ${_src})
generate_export_header(
  ${_name}
  EXPORT_MACRO_NAME ${_name}_EXPORT
  EXPORT_FILE_NAME Export.h
  )
target_link_libraries(${_name} PUBLIC OgreMain ${VTK_LIBRARIES})
vtk_module_autoinit(TARGETS ${_name} MODULES ${VTK_LIBRARIES})

## eof - CMakeLists.txt
