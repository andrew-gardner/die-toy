CMAKE_MINIMUM_REQUIRED(VERSION 2.8.8)
PROJECT(dieToy)


# // QT5 //
#
# Tell CMake to run moc when necessary:
SET(CMAKE_AUTOMOC ON)

# Tell CMake to auto-embed all the QRCs
SET(CMAKE_AUTORCC ON)

# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
SET(CMAKE_INCLUDE_CURRENT_DIR ON)

# Widgets finds its own dependencies (QtGui and QtCore).
FIND_PACKAGE(Qt5Widgets REQUIRED)

# The Qt5Widgets_INCLUDES also includes the include directories for
# dependencies QtCore and QtGui
INCLUDE_DIRECTORIES(${Qt5Widgets_INCLUDE_DIRS})

# We need add -DQT_WIDGETS_LIB when using QtWidgets in Qt 5.
ADD_DEFINITIONS(${Qt5Widgets_DEFINITIONS})

# Executables fail to build with Qt 5 in the default configuration
# without -fPIE. We add that here.
SET(CMAKE_CXX_FLAGS "${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
#
# // QT5 //


set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}")
FIND_PACKAGE(OpenCV REQUIRED)
INCLUDE_DIRECTORIES(${OpenCV2_INCLUDE_DIRS})

FIND_PACKAGE(OpenMP)
IF (OPENMP_FOUND)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
ENDIF()

SET(SOURCEFILES 
	src/main.cpp 
	src/MainWindow.cpp 
	src/DrawWidget.cpp)
ADD_EXECUTABLE(dieToy ${SOURCEFILES})
TARGET_LINK_LIBRARIES(dieToy ${OpenCV2_LIBRARIES} ${Qt5Widgets_LIBRARIES})
