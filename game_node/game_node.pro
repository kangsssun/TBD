QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

win32 {
    QMAKE_CFLAGS += /utf-8
    QMAKE_CXXFLAGS += /utf-8
}

unix {
    QMAKE_CFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
    QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
}

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += $$PWD

SOURCES += \
    main.cpp \
    game_node.cpp \
    keyboard/keyboardpanel.cpp \
    title/teamnamedialog.cpp \
    intro/emergencypage.cpp \
    game/readypage.cpp \
    game/missionpage.cpp \
    chat/contactgmdialog.cpp \
    notice/noticedialog.cpp

HEADERS += \
    game_node.h \
    keyboard/keyboardpanel.h \
    title/teamnamedialog.h \
    intro/emergencypage.h \
    game/readypage.h \
    game/missionpage.h \
    chat/contactgmdialog.h \
    notice/noticedialog.h

FORMS += \
    game_node.ui

RESOURCES += \
    resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
