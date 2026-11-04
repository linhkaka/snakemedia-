#
# Jenny Script fork: update check and install from the GitHub releases of the fork.
# Built on every platform so the Linux and macOS CI jobs compile it too, it is only used on Windows.
#
HEADERS += \
    $$PWD/app_update_manager.h \
    $$PWD/update_info.h

SOURCES += \
    $$PWD/app_update_manager.cpp \
    $$PWD/update_info.cpp

RESOURCES += $$PWD/update.qrc
