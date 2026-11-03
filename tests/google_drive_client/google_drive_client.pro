TEMPLATE = app
TARGET = tst_google_drive_client
CONFIG += testcase c++17 console
CONFIG -= app_bundle
QT += network testlib
QT -= gui

GOOGLE_DIR = $$PWD/../../src/core/management_layer/content/google
INCLUDEPATH += $$GOOGLE_DIR

HEADERS += $$GOOGLE_DIR/google_drive_client.h
SOURCES += \
    $$GOOGLE_DIR/google_drive_client.cpp \
    tst_google_drive_client.cpp
