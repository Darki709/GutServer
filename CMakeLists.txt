if(MSVC)
	add_compile_options(/FS)
endif()

cmake_minimum_required(VERSION 3.10)
project(
	gutweb
	VERSION 1.0
	LANGUAGES C CXX)

# =========================
# C++ STANDARD
# =========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# PYTHON (EMBEDDED)
find_package(Python3 REQUIRED
	COMPONENTS Interpreter Development
)
set(Python3_INCLUDE_DIR "C:/Program Files/WindowsApps/PythonSoftwareFoundation.Python.3.13_3.13.2544.0_x64__qbz5n2kfra8p0/include")
set(Python3_LIBRARY_RELEASE "C:/Program Files/WindowsApps/PythonSoftwareFoundation.Python.3.13_3.13.2544.0_x64__qbz5n2kfra8p0/libs/python313.lib")

file(GLOB_RECURSE CPP_SOURCES
	src/**/*.cpp
	src/**/**/*.cpp
	src/*.cpp
)

# Copy your python scripts to the build folder whenever they change
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/src/stockapi/stockapi.py"
	DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(gutweb
	${CPP_SOURCES}
	${CMAKE_CURRENT_SOURCE_DIR}/src/external/sqlite3.c
)

# =========================
# INCLUDES
# =========================
set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")
set(OPENSSL_INCLUDE_DIR "C:/Program Files/OpenSSL-Win64/include")
find_package(OpenSSL REQUIRED)
target_include_directories(gutweb
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
target_link_libraries(gutweb
	PRIVATE
	ws2_32
	OpenSSL::SSL OpenSSL::Crypto
	${Python3_LIBRARY_RELEASE}
)

# =========================
# COPY PYTHON FILES NEXT TO EXE
# =========================
add_custom_command(TARGET gutweb POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${PROJECT_SOURCE_DIR}/src/stockapi
	$<TARGET_FILE_DIR:gutweb>/stockapi
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"C:/Program Files/OpenSSL-Win64/bin/libcrypto-3-x64.dll"
	$<TARGET_FILE_DIR:${PROJECT_NAME}>)
