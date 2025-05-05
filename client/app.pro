QT += core gui network charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = monitoring-client
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    httpmanager.cpp

HEADERS += \
    mainwindow.h \
    httpmanager.h \
    datapoint.h
