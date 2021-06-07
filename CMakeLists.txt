cmake_minimum_required (VERSION 3.7)
project (libbice
	VERSION 0.0.1
	LANGUAGES C)
set(PROJECT_DESCRIPTION "libbice is a UDP ICE library, include signalling channel")
set(CMAKE_C_FLAGS "-Wall -O0 -g ${CC_WARNING_FLAGS} ${CMAKE_C_FLAGS}")


if(MSVC)
	option(NO_ATOMICS "Force disabling C11 atomics" ON)
else()
	option(NO_ATOMICS "Force disabling C11 atomics" OFF)
endif()

set(C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)

if(WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
	if (MSVC)
		add_definitions(-DNOMINMAX)
		add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	endif()
endif()

set(LIBBICE_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/bp2p_ice_api.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/ice_client.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/ice_peer.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/ice_common.c
)

set(LIBBICE_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/include/libbice/bp2p_ice_api.h
)

#set(TESTS_SOURCES
#    ${CMAKE_CURRENT_SOURCE_DIR}/test/main.c
#    ${CMAKE_CURRENT_SOURCE_DIR}/test/crc32.c
#    ${CMAKE_CURRENT_SOURCE_DIR}/test/base64.c
#)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(bice STATIC ${LIBBICE_SOURCES})
#set_target_properties(bice PROPERTIES VERSION ${PROJECT_VERSION})

set(LIBEV_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libev)
set(LIBUMQTT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libumqtt/include/umqtt)
set(LIBJUICE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libjuice/include)
set(LIBJUICE_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libjuice)
#target_include_directories(bice PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(bice PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/libbice)
#target_include_directories(bice PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(bice PRIVATE ${LIBEV_INCLUDE_DIR})
target_include_directories(bice PRIVATE ${LIBUMQTT_INCLUDE_DIR})
target_include_directories(bice PRIVATE ${LIBJUICE_INCLUDE_DIR})

target_link_libraries(bice PUBLIC Threads::Threads)


install(TARGETS bice  DESTINATION lib)
install(FILES ${LIBBICE_HEADERS} DESTINATION include)



set(LIBEV_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libev/include)
set(LIBEV_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libev/lib)
set(LIBUMQTT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libumqtt/include/umqtt)
set(LIBUMQTT_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libumqtt/lib)

link_directories(p2pclient PRIVATE ${LIBEV_LIB_DIR} ${LIBUMQTT_LIB_DIR} ${LIBJUICE_LIB_DIR})
#link_directories(juice-peer PRIVATE ${LIBEV_LIB_DIR} ${LIBUMQTT_LIB_DIR})


# Samples
# p2pclient
add_executable(p2pclient ${CMAKE_CURRENT_SOURCE_DIR}/sample/p2pclient.c)
target_include_directories(p2pclient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/libbice)
target_include_directories(p2pclient PRIVATE ${LIBEV_INCLUDE_DIR})
target_include_directories(p2pclient PRIVATE ${LIBUMQTT_INCLUDE_DIR})
target_link_libraries(p2pclient bice juice ev umqtt)

# p2ppeer
add_executable(p2ppeer ${CMAKE_CURRENT_SOURCE_DIR}/sample/p2ppeer.c)
target_include_directories(p2ppeer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/libbice)
target_include_directories(p2ppeer PRIVATE ${LIBEV_INCLUDE_DIR})
target_include_directories(p2ppeer PRIVATE ${LIBUMQTT_INCLUDE_DIR})
target_link_libraries(p2ppeer bice juice ev umqtt)

