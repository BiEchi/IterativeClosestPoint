project(Exercise2)

cmake_minimum_required(VERSION 2.8)

# setup output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# additional CMake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# setup FreeGLUT
find_package(FreeGLUT)
include_directories(${FREEGLUT_INCLUDE_DIR})
if(NOT FREEGLUT_FOUND)
    message(ERROR " FREEGLUT not found")
endif()

# setup OpenGL
find_package(OpenGL)
include_directories(${OpenGL_INCLUDE_DIRS})
if(NOT OPENGL_FOUND)
    message(ERROR " OpenGL not found")
endif()

# setup OpenMesh
find_package(OpenMesh)
include_directories(${OPENMESH_INCLUDE_DIRS})
if(NOT OPENMESH_FOUND)
    message(ERROR " OpenMesh not found")
endif()
set_property(
    DIRECTORY
    APPEND PROPERTY COMPILE_DEFINITIONS _USE_MATH_DEFINES
)

# collect sources
file(GLOB all_sources *.cc)
file(GLOB all_headers *.hh)

file(GLOB ann_sources ANN/*.cpp)
file(GLOB ann_headers ANN/*.h)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# compile and link
add_library(ann ${ann_sources} ${ann_headers})
add_executable(exercise2 ${all_sources} ${all_headers})
target_link_libraries(exercise2 debug ${OPENMESH_CORE_DEBUG_LIBRARY} debug ${OPENMESH_TOOLS_DEBUG_LIBRARY} optimized ${OPENMESH_CORE_LIBRARY} optimized  ${OPENMESH_TOOLS_LIBRARY} ${OPENGL_LIBRARIES}  ${FREEGLUT_LIBRARY} ann)

ADD_CUSTOM_COMMAND (TARGET exercise2 POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different $<$<CONFIG:Debug>:${FREEGLUT_INCLUDE_DIR}/../bin/freeglut.dll> $<$<NOT:$<CONFIG:Debug>>:${FREEGLUT_INCLUDE_DIR}/../bin/freeglut.dll> $<TARGET_FILE_DIR:exercise2>
COMMAND ${CMAKE_COMMAND} -E copy_if_different $<$<CONFIG:Debug>:${OPENMESH_INCLUDE_DIRS}/../OpenMeshCored.dll> $<$<NOT:$<CONFIG:Debug>>:${OPENMESH_INCLUDE_DIRS}/../OpenMeshCore.dll> $<TARGET_FILE_DIR:exercise2>  
COMMAND ${CMAKE_COMMAND} -E copy_if_different $<$<CONFIG:Debug>:${OPENMESH_INCLUDE_DIRS}/../OpenMeshToolsd.dll> $<$<NOT:$<CONFIG:Debug>>:${OPENMESH_INCLUDE_DIRS}/../OpenMeshTools.dll> $<TARGET_FILE_DIR:exercise2>
# Output Message
COMMENT "Copying dependent dll" VERBATIM)