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

    SOURCES += \
        dd_api/led_api.c \
        dd_api/camera_api.c \
        dd_api/buzzer_api.c \
        dd_api/thermal_api.c \
        dd_api/zyro_api.c

    HEADERS += \
        dd_api/led_api.h \
        dd_api/led_ioctl.h \
        dd_api/camera_api.h \
        dd_api/buzzer_api.h \
        dd_api/buzzer_ioctl.h \
        dd_api/thermal_api.h \
        dd_api/zyro_api.h
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
    game/mission1.cpp \
    game/mission2.cpp \
    game/mission3.cpp \
    game/mission4.cpp \
    game/mission5.cpp \
    chat/contactgmdialog.cpp \
    notice/noticedialog.cpp

HEADERS += \
    game_node.h \
    keyboard/keyboardpanel.h \
    title/teamnamedialog.h \
    intro/emergencypage.h \
    game/readypage.h \
    game/missionpage.h \
    game/missionpage_utils.h \
    chat/contactgmdialog.h \
    notice/noticedialog.h

FORMS += \
    game_node.ui

RESOURCES += \
    resources.qrc

LIBS += \
    -ljpeg

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
