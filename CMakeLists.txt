cmake_minimum_required(VERSION 3.2)
set(PROJECT_NAME GutServer)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_TARGET_TRIPLET "x64-windows-static")

project(
	${PROJECT_NAME}
	VERSION 1.0
	LANGUAGES C CXX)

# =========================
# C++ STANDARD
# =========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_link_options(-static-libgcc -static-libstdc++ -pthread)
endif()

if(MSVC)
	# Remove /Zi (which uses the shared PDB) and add /Z7 (which puts debug info in each .obj)
	string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")

	# Also add /FS just as a secondary safety measure
	add_compile_options(/FS)

	message(STATUS "DEBUG: Switched to /Z7 format to bypass all PDB locking errors.")
endif()

# PYTHON (EMBEDDED)
set(PYTHON_313_ROOT "C:/Users/NASASuperPC/AppData/Local/Programs/Python/Python313")

# Point to the headers (.h files)
set(Python3_INCLUDE_DIR "${PYTHON_313_ROOT}/include")
set(Python3_LIBRARIES "${PYTHON_313_ROOT}/libs/python313.lib")

file(GLOB_RECURSE CPP_SOURCES
	src/**/*.cpp
	src/**/**/*.cpp
	src/*.cpp
)

# Copy your python scripts to the build folder whenever they change
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/src/stockapi/stockapi.py"
	DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(${PROJECT_NAME}
	${CPP_SOURCES}
	${CMAKE_CURRENT_SOURCE_DIR}/src/external/sqlite3.c
)

# =========================
# INCLUDES
# =========================
# set(OPENSSL_INCLUDE_DIR "C:/Program Files/OpenSSL-Win64/include")
# set(OPENSSL_USE_STATIC_LIBS TRUE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
target_include_directories(${PROJECT_NAME}
	PRIVATE
	${PROJECT_SOURCE_DIR}/src
	${PROJECT_SOURCE_DIR}/src/logic
	${PROJECT_SOURCE_DIR}/src/runtime
	${PROJECT_SOURCE_DIR}/src/program
	${PROJECT_SOURCE_DIR}/src/stateless
	${PROJECT_SOURCE_DIR}/src/external
	${OPENSSL_INCLUDE_DIR}
	${Python3_INCLUDE_DIR}
)

# =========================
# LINKING
# =========================
target_link_libraries(${PROJECT_NAME}
	PRIVATE
	Ws2_32.lib
	Crypt32.lib
	Advapi32.lib
	User32.lib
	OpenSSL::SSL OpenSSL::Crypto
	CURL::libcurl
	${Python3_LIBRARIES}
)

# =========================
# COPY PYTHON FILES NEXT TO EXE
# =========================
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${PROJECT_SOURCE_DIR}/src/stockapi
	$<TARGET_FILE_DIR:${PROJECT_NAME}>/stockapi
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"C:/Program Files/OpenSSL-Win64/bin/libcrypto-3-x64.dll"
	$<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD

	# Create the folder and __init__.py first
	COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/stockapi
	COMMAND ${CMAKE_COMMAND} -E touch $<TARGET_FILE_DIR:${PROJECT_NAME}>/stockapi/__init__.py

	# Copy the actual scripts
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"${PROJECT_SOURCE_DIR}/src/stockapi"
	"$<TARGET_FILE_DIR:${PROJECT_NAME}>/stockapi"
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"${PYTHON_313_ROOT}/python313.dll"
	"$<TARGET_FILE_DIR:${PROJECT_NAME}>/python313.dll"
)
