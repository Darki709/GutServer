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
)

add_executable(gutweb
    gutweb.cpp
    ${CPP_SOURCES}
)

# =========================
# INCLUDES
# =========================
target_include_directories(gutweb
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src
)

# =========================
# LINKING
# =========================
target_link_libraries(gutweb
    PRIVATE Python3::Python
            ws2_32
)


# =========================
# COPY PYTHON FILES NEXT TO EXE
# =========================
add_custom_command(TARGET gutweb POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/src/stockapi
        $<TARGET_FILE_DIR:gutweb>/stockapi
)
