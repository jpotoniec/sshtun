TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += spdlog/include/
LIBS += -lpthread

SOURCES += \
    sshtun.cpp \
    Tunnel.cpp \
    IniFile.cpp \
    PrivilegedOperations.cpp \
    Config.cpp \
    Register.cpp

include(deployment.pri)
qtcAddDeployment()

HEADERS += \
    Buffer.hpp \
    LibcError.hpp \
    Tunnel.hpp \
    Config.hpp \
    IniFile.hpp \
    Logger.hpp \
    PrivilegedOperations.hpp \
    Utils.hpp \
    Register.hpp

OTHER_FILES += \
    sshtun.ini \
    fruwacz.ini

