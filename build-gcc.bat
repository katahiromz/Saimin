g++ -c -O3 -DUNICODE -D_UNICODE Saimin.cpp -o Saimin.o
windres -i Saimin_res.rc -o Saimin_res.o
g++ -mwindows -O3 -o Saimin Saimin.o Saimin_res.o -lshlwapi
strip Saimin.exe
del *.o
