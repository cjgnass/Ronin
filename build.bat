@echo off
slangc shader.slang -target spirv -profile glsl_460 -matrix-layout-column-major -o shader.spv
if errorlevel 1 exit /b %errorlevel%
odin build . -debug
if errorlevel 1 exit /b %errorlevel%
Ronin.exe
