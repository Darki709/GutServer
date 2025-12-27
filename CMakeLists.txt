if (MSVC)
    add_compile_options(/FS)
endif()

cmake_minimum_required(VERSION 3.10)
project(
	gutweb
	VERSION 1.0
	LANGUAGES CXX)


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

file(GLOB_RECURSE CPP_SOURCES
    src/**/*.cpp
	src/**/**/*.cpp
	src/*.cpp
)

add_executable(gutweb
    ${CPP_SOURCES}
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
		${OPENSSL_INCLUDE_DIR}
)

# =========================
# LINKING
# =========================
target_link_libraries(gutweb
    PRIVATE Python3::Python
            ws2_32
			OpenSSL::SSL OpenSSL::Crypto
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
