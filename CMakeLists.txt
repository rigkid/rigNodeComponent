file(READ "${CMAKE_CURRENT_LIST_DIR}/pack.json" PACK_JSON)
string(JSON PACK_NAME GET "${PACK_JSON}" name)

if(NOT TARGET rigComponent)
	if(DEFINED RIGKIT_ROOT AND NOT RIGKIT_ROOT STREQUAL "")
		set(_rigcomponent_dir "${RIGKIT_ROOT}/packs/rigComponent")
	else()
		set(_rigcomponent_dir "${CMAKE_SOURCE_DIR}/packs/rigComponent")
	endif()
	if(EXISTS "${_rigcomponent_dir}/CMakeLists.txt")
		add_subdirectory("${_rigcomponent_dir}" "${CMAKE_BINARY_DIR}/packs/rigComponent")
	else()
		message(FATAL_ERROR "rigNodeComponent requires rigComponent under packs/rigComponent")
	endif()
endif()

add_library(${PACK_NAME} STATIC
	src/rigNodeComponent.cpp
	src/rigNodeComponent.h
	src/CNodeGraph.cpp
	src/CNodeGraph.h
	src/NodeGraph.cpp
	src/NodeGraph.h
)
target_link_libraries(${PACK_NAME} PUBLIC rigkit rigComponent)
target_include_directories(${PACK_NAME} PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>
)

message(STATUS "Configured pack: ${PACK_NAME} (node graph data + helpers)")
