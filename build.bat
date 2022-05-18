bcc32 -WC -DINITGUID -I../freeglut-MinGW-3.0.0-1.mp/include -I"C:/Program Files (x86)/Microsoft Speech SDK 5.1/Include" -WC Saimin.cpp tess.cpp opengl32.lib glu32.lib freeglut.lib shlwapi.lib winmm.lib
brc32 Saimin_res.rc Saimin.exe
