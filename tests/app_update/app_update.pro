TEMPLATE = app
TARGET = tst_app_update
CONFIG += testcase c++17 console
CONFIG -= app_bundle
QT += testlib
QT -= gui

UPDATE_DIR = $$PWD/../../src/core/management_layer/content/update
INCLUDEPATH += $$UPDATE_DIR

HEADERS += $$UPDATE_DIR/update_info.h
SOURCES += \
    $$UPDATE_DIR/update_info.cpp \
    tst_app_update.cpp
