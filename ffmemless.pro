TEMPLATE = lib
CONFIG += qt plugin hide_symbols
QT = core
TARGET = $$qtLibraryTarget(qtfeedback_ffmemless)
PLUGIN_TYPE=feedback

HEADERS += qfeedback.h
SOURCES += qfeedback.cpp
OTHER_FILES += ffmemless.ini
#DEFINES += FF_MEMLESS_LRA # FF_MEMLESS_ERM # FF_MEMLESS_PIEZO
DEFINES += 'FF_MEMLESS_SETTINGS=\'\"$$[QT_INSTALL_PLUGINS]/feedback/ffmemless.ini\"\''

settings.files = ffmemless.ini
settings.path = $$[QT_INSTALL_PLUGINS]/feedback/
target.path = $$[QT_INSTALL_PLUGINS]/feedback
INSTALLS += target settings

OTHER_FILES += ffmemless.json
QT += feedback
plugindescription.files = ffmemless.json
plugindescription.path = $$[QT_INSTALL_PLUGINS]/feedback/
INSTALLS += plugindescription

# also enable profile detection. libprofile-qt5 is a bit broken, work around it here.
QT += dbus
QMAKE_CXXFLAGS += -I/usr/include/profile-qt5
QMAKE_LFLAGS += -lprofile-qt5
