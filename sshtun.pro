TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    sshtun.cpp

include(deployment.pri)
qtcAddDeployment()

