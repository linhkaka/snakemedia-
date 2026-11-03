TEMPLATE = app
TARGET = tst_fountain_import
CONFIG += testcase c++17 console
CONFIG -= app_bundle
QT += widgets testlib

INCLUDEPATH += $$PWD/../../src $$PWD/../../src/corelib
STARC_BUILD = $$clean_path($$PWD/../../src/_build)
mac {
    CORELIB_DIR = $$STARC_BUILD/starcapp.app/Contents/Frameworks
} else {
    CORELIB_DIR = $$STARC_BUILD
}
LIBS += -L$$CORELIB_DIR -lcorelib
QMAKE_RPATHDIR += $$CORELIB_DIR $$STARC_BUILD/libs
SOURCES += tst_fountain_import.cpp
