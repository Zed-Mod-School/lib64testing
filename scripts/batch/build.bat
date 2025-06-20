@echo off
setlocal

:: Path to Git Bash (from Scoop)
set "GIT_BASH=C:\Users\user\scoop\apps\git\current\git-bash.exe"

:: Verify Git Bash exists
if not exist "%GIT_BASH%" (
    echo Git Bash not found at "%GIT_BASH%"
    pause
    exit /b 1
)

:: Run Git Bash in current dir and copy the DLL
start "" "%GIT_BASH%" --cd="%CD%" -c "make && mkdir -p ../../../out/build/Release/bin ../../../out/build/Debug/bin && cp dist/sm64.dll ../../../out/build/Release/bin/ && cp dist/sm64.dll ../../../out/build/Debug/bin/; echo; echo Press any key to close...; "
