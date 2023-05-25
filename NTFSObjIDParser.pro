#-------------------------------------------------
#
# Project created by QtCreator 2016-11-25T17:57:05
#
#-------------------------------------------------

QT       += core gui


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QMAKE_MAC_SDK = macosx11.3

TARGET = NTFSObjIDParser
TEMPLATE = app
ICON = MyIcon.icns

SOURCES += main.cpp\
        mainwindow.cpp \
    Source.cpp

HEADERS  += mainwindow.h \
    structures.h

FORMS    += mainwindow.ui \
    runinstallerform.ui
