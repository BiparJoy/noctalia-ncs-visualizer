cmake_minimum_required(VERSION 3.20)
project(aurora-ncs VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Quick OpenGL)
find_package(LayerShellQt REQUIRED)

qt_standard_project_setup(REQUIRES 6.6)

qt_add_executable(aurora-ncs
    src/main.cpp
    src/motion.h
    src/motion.cpp
    src/cava.h
    src/cava.cpp
    src/driver.h
    src/driver.cpp
)

# The GPU renderer from the Plasma widget, compiled into the executable so
# nothing has to be installed into Qt's system QML path (no sudo).
qt_add_qml_module(aurora-ncs
    URI AuroraNcs
    VERSION 1.0
    QML_FILES src/Orb.qml
    SOURCES
        src/ncsvisualizer.h
        src/ncsvisualizer.cpp
        src/shaders.h
)

target_include_directories(aurora-ncs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(aurora-ncs PRIVATE AURORA_NCS_VERSION="${PROJECT_VERSION}")
target_link_libraries(aurora-ncs PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::OpenGL
    LayerShellQt::Interface
)

install(TARGETS aurora-ncs RUNTIME DESTINATION bin)
