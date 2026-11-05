## =========================================================================
## @author Leonardo Florez-Valencia (florez-l@javeriana.edu.co)
## =========================================================================

add_executable(
  pujOgre_SimpleParametricExpressionViewer
  Application.cxx
  main.cxx
  )
target_link_libraries(
  pujOgre_SimpleParametricExpressionViewer
  PRIVATE OgreBites
  )

list(JOIN pujOgre_PLUGINS "|" _plugins)
target_compile_definitions(
  pujOgre_SimpleParametricExpressionViewer
  PRIVATE
  pujOgre_PLUGINS="${_plugins}"
  pujOgre_SHARED_EXT="${CMAKE_SHARED_LIBRARY_SUFFIX}"
  )

## eof - CMakeLists.txt
