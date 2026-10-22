@echo off
set "JUCE_ROOT=E:\05 BACKUP\CARTELLE\JUCE"
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;C:\Users\Whyed\AppData\Roaming\Python\Python312\Scripts;C:\Users\Whyed\AppData\Local\Programs\Python\Python312\Scripts;%PATH%"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d E:\Dev\WhyCremisi-verifica-carlo
cmake -S . -B build-v -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_UNIT_TESTS=ON "-DJUCE_WEBVIEW2_PACKAGE_LOCATION=E:/05 BACKUP/CARTELLE/NuGet" || exit /b 1
cmake --build build-v --target WhyCremisiTests -j 4 || exit /b 2
echo BUILD-FINITA
for /r build-v %%f in (WhyCremisiTests.exe) do if exist "%%f" ("%%f" && echo TEST-FINITI)
