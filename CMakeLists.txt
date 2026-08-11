file(READ "${CMAKE_CURRENT_LIST_DIR}/pack.json" PACK_JSON)
string(JSON PACK_NAME GET "${PACK_JSON}" name)

if(NOT TARGET rigComponent)
	if(DEFINED RIGKIT_ROOT AND NOT RIGKIT_ROOT STREQUAL "")
		set(_rigComponent_dir "${RIGKIT_ROOT}/packs/rigComponent")
	else()
		set(_rigComponent_dir "${CMAKE_SOURCE_DIR}/packs/rigComponent")
	endif()
	if(EXISTS "${_rigComponent_dir}/CMakeLists.txt")
		add_subdirectory("${_rigComponent_dir}" "${CMAKE_BINARY_DIR}/packs/rigComponent")
	else()
		message(FATAL_ERROR "rigNodeComponent requires rigComponent under packs/rigComponent")
	endif()
endif()

if(NOT TARGET rigProject)
	if(DEFINED RIGKIT_ROOT AND NOT RIGKIT_ROOT STREQUAL "")
		set(_rigProject_dir "${RIGKIT_ROOT}/packs/rigProject")
	else()
		set(_rigProject_dir "${CMAKE_SOURCE_DIR}/packs/rigProject")
	endif()
	if(EXISTS "${_rigProject_dir}/CMakeLists.txt")
		add_subdirectory("${_rigProject_dir}" "${CMAKE_BINARY_DIR}/packs/rigProject")
	else()
		message(FATAL_ERROR "rigNodeComponent requires rigProject under packs/rigProject")
	endif()
endif()

add_library(${PACK_NAME} STATIC
	src/rigNodeComponent.cpp
	src/rigNodeComponent.h
	src/CNodeGraph.cpp
	src/CNodeGraph.h
	src/NodeGraph.cpp
	src/NodeGraph.h
	src/GraphEval.cpp
	src/GraphEval.h
	src/ApplyRefs.cpp
	src/ApplyRefs.h
	src/NodeCatalog.cpp
	src/NodeCatalog.h
	src/NodeSerializers.cpp
	src/NodeSerializers.h
	src/SGraphEval.cpp
	src/SGraphEval.h
)
target_link_libraries(${PACK_NAME} PUBLIC rigkit)
if(TARGET rigComponent)
	target_link_libraries(${PACK_NAME} PUBLIC rigComponent)
endif()
if(TARGET rigProject)
	target_link_libraries(${PACK_NAME} PUBLIC rigProject)
endif()
target_include_directories(${PACK_NAME} PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>
)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/tools/node_smoke.cpp")
	add_executable(node_smoke "${CMAKE_CURRENT_LIST_DIR}/tools/node_smoke.cpp")
	target_link_libraries(node_smoke PRIVATE ${PACK_NAME})
endif()

message(STATUS "Configured pack: ${PACK_NAME} (node graph data + helpers)")
