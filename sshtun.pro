TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    sshtun.cpp \
    Tunnel.cpp \
    IniFile.cpp

include(deployment.pri)
qtcAddDeployment()

HEADERS += \
    Buffer.hpp \
    LibcError.hpp \
    Tunnel.hpp \
    Config.hpp \
    IniFile.hpp

