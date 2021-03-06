set(CMAKE_SYSTEM_PROCESSOR "i686") # 32-bit build required for FIST

# >= 1.47.0 for polygon and geometry packages
# >= 1.54.0 for boost::geomtry::index::rtree
# >= 1.59.0 for boost::geometry::index::rtree::const_iterator
find_package(Boost 1.54.0 REQUIRED)

find_package(FIST REQUIRED)

set(CXX_STATIC "-static-libgcc -static-libstdc++ ")
if (GCC_VERSION VERSION_GREATER 5.0 AND GCC_VERSION VERSION_LESS 5.4)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_STATIC}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CXX_STATIC}")
endif()

# annoying warnings embedded in boost library code for recent gcc
if (GCC_VERSION VERSION_GREATER 7.0)
  add_definitions(-Wno-class-memaccess)
endif()

# for debugging to dump cmake environment variables
if (${CMAKE_DEBUG})
  get_cmake_property(_variableNames VARIABLES)
  foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
  endforeach()
endif()

if (${DEBUG})
  add_definitions(-DMAXRECT_DEBUG -DMAXTRI_DEBUG)
endif()

set(LIB_INCLUDE "${CMAKE_SOURCE_DIR}/lib")

link_directories(
  ${LIB_INCLUDE}
  ${FIST_LIBRARY_DIR}
)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/shapefile
  ${CMAKE_CURRENT_SOURCE_DIR}/shapelib
  ${Boost_INCLUDE_DIRS}
  ${LIB_INCLUDE}
  )

add_subdirectory(shapefile)
add_subdirectory(shapelib)

set(TEST_PROG voronoi-game)

add_executable(${TEST_PROG}
  boost_geo_poly.h
  util.h

  sweep.h
  nn1.h
  voronoi.h
  voronoi.cc
  maxtri.h
  maxtri.cc
  maxrect.h
  maxrect.cc

  vgame.h
  #vgame.cc

  user.h
  user.cc

  main.h
  main.cc
)

# these are just annoying
add_definitions(-Wno-parentheses)

target_link_libraries(${TEST_PROG}
  ${Boost_LIBRARIES}
  ${FIST_LIBRARY}
  shapefile shapelib
)

#cotire(${TEST_PROG})

install (TARGETS ${TEST_PROG} DESTINATION bin)
