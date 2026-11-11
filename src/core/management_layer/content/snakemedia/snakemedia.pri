# Native SNAKEMEDIA production workspace; Qt modules already used by core.
HEADERS += $$PWD/production_brief.h $$PWD/production_dialog.h
SOURCES += $$PWD/production_brief.cpp $$PWD/production_dialog.cpp
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
