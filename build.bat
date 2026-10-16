@echo off
rem Builds GhostMode.asi with zig (https://ziglang.org - or: pip install ziglang, then use "python -m ziglang cc").
cd /d "%~dp0"
zig cc -target x86_64-windows-gnu -shared -O2 -s -o GhostMode.asi src\ghostmode.c || python -m ziglang cc -target x86_64-windows-gnu -shared -O2 -s -o GhostMode.asi src\ghostmode.c
del /q *.lib 2>nul
if exist GhostMode.asi (echo Built GhostMode.asi) else (echo BUILD FAILED)
