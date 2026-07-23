QT += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11 console
CONFIG -= app_bundle

TARGET = QT_ConsoleTest
TEMPLATE = app

HEADERS += \
    finstcp.h

SOURCES += \
    finstcp.cpp \
    main.cpp
