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

# This forces the use of the static C++ runtime
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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

add_executable(gutweb
	${CPP_SOURCES}
	${CMAKE_CURRENT_SOURCE_DIR}/src/external/sqlite3.c
)

# =========================
# INCLUDES
# =========================
set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")
set(OPENSSL_INCLUDE_DIR "C:/Program Files/OpenSSL-Win64/include")
set(OPENSSL_USE_STATIC_LIBS TRUE)
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
	${Python3_LIBRARIES}
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
add_custom_command(TARGET gutweb POST_BUILD
    # Create the folder and __init__.py first
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:gutweb>/stockapi
    COMMAND ${CMAKE_COMMAND} -E touch $<TARGET_FILE_DIR:gutweb>/stockapi/__init__.py
    # Copy the actual scripts
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROJECT_SOURCE_DIR}/src/stockapi"
    "$<TARGET_FILE_DIR:gutweb>/stockapi"
)
add_custom_command(TARGET gutweb POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PYTHON_313_ROOT}/python313.dll"
    "$<TARGET_FILE_DIR:gutweb>/python313.dll"
)
