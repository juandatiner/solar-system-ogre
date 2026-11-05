## =========================================================================
## @author Leonardo Florez-Valencia (florez-l@javeriana.edu.co)
## =========================================================================

cmake_minimum_required(VERSION 3.20)
project(pujOgre LANGUAGES CXX VERSION 1.0.0.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

include(GenerateExportHeader)

find_package(OGRE REQUIRED)
find_package(
  VTK QUIET
  COMPONENTS
  CommonCore CommonDataModel CommonComputationalGeometry CommonMisc
  FiltersCore FiltersSources IOLegacy IOXML
  )

set(
  pujOgre_PLUGINS
  Plugin_pujOgreDotPPSceneCodec
  Plugin_pujOgreVTKCodec
  )

subdirs(apps lib/pujOgre plugins)

## eof - CMakeLists.txt
