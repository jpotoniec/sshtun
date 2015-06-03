TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    sshtun.cpp \
    Tunnel.cpp

include(deployment.pri)
qtcAddDeployment()

HEADERS += \
    Buffer.hpp \
    LibcError.hpp \
    Tunnel.hpp

