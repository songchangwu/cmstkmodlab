######################################################################
# Automatically generated by qmake (2.01a) Tue Apr 24 13:51:04 2012
######################################################################

LIBS += -L/Users/mussgill/Documents/Physik/CMS/Labor/cmstkmodlab/devices/lib -lTkModLabConrad

macx { 
    INCLUDEPATH += /opt/local/include/GL
    LIBS += -L/opt/local/lib
    DEFINES += USE_FAKEIO
}
else { 
    LIBS += -L/usr/lib64
}


TEMPLATE = app
TARGET = conrad
DEPENDPATH += .
INCLUDEPATH += .
INCLUDEPATH += ../

# Input
HEADERS += ConradMainWindow.h
FORMS += ConradControl.ui
SOURCES += conrad.cc ConradMainWindow.cc
