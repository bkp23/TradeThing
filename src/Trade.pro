#-------------------------------------------------
#
# Project created by QtCreator 2016-01-07T15:36:40
#
#-------------------------------------------------

QT       += core gui concurrent network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Trade
TEMPLATE = app

RC_ICONS = trade.ico

SOURCES += main.cpp\
        mainwindow.cpp \
    parser.cpp \
    graph.cpp \
    heap.cpp \
    javarand.cpp \
    exec.cpp \
    metric.cpp

HEADERS  += mainwindow.h \
    parser.h \
    graph.h \
    heap.h \
    javarand.h \
    exec.h \
    metric.h

FORMS    += mainwindow.ui
