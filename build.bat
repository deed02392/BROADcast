@echo off

gcc.exe -O2 -municode -DUNICODE broadcast.c -o broadcast.exe -lws2_32 -lIphlpapi
