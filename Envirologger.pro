QT       += core gui serialport
QT       += printsupport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
LIBS += -lPsapi
CONFIG -= app_bundle

# Include QXlsx header and source files
include(./QXlsx/QXlsx.pri)

# kissFFT (single correct reference)
INCLUDEPATH += $$PWD/kissfft
SOURCES += $$PWD/kissfft/kiss_fft.c

# If you have the internal guts header, keep it in the folder; no need to list it in HEADERS
# HEADERS should list project headers only (optional to include kissfft headers)
HEADERS += \
    enlargeplot.h \
    mainwindow.h \
    qcustomplot.h \
    serialporthandler.h

SOURCES += \
    enlargeplot.cpp \
    main.cpp \
    mainwindow.cpp \
    qcustomplot.cpp \
    serialporthandler.cpp

FORMS += \
    enlargeplot.ui \
    mainwindow.ui

QT += xml

DEFINES += QT_DEPRECATED_WARNINGS

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
