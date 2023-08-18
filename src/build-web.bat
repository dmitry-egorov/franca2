@echo off
rem Builds the Emscripten implementation (on Windows)
rem TODO: CMake...
rem

xcopy assets\*.* embedded\. /Y /q
xcopy *.wgsl embedded\. /Y /q

if "%~1"=="/d" (
  set DEBUG=true
) else (
  set DEBUG=false
)

set CPP_FLAGS=-std=c++20 -Wall -Wextra -fno-exceptions -fno-rtti
set EMS_FLAGS=--output_eol linux -s ALLOW_MEMORY_GROWTH=0 -s INITIAL_MEMORY=16MB -s ENVIRONMENT=web -s NO_EXIT_RUNTIME=1 -s STRICT=1 -s TEXTDECODER=2 -s USE_WEBGPU=1 -s WASM=1 -msse2 -msimd128 --shell-file shell.html --embed-file embedded
set OPT_FLAGS=

if %DEBUG%==true (
  xcopy *.cpp out\. /Y /q
  xcopy *.h out\. /Y /q
  xcopy utility\*.h out\utility\. /Y /q

  set CPP_FLAGS=%CPP_FLAGS% -g3 -D_DEBUG=1 -Wno-unused -Wno-limited-postlink-optimizations
  set EMS_FLAGS=%EMS_FLAGS% -s ASSERTIONS=1 -s DEMANGLE_SUPPORT=1 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=2 -gsource-map
  set OPT_FLAGS=%OPT_FLAGS% -O0
) else (
  set CPP_FLAGS=%CPP_FLAGS% -g0 -DNDEBUG=1 -flto -Werror
  set EMS_FLAGS=%EMS_FLAGS% -s ABORTING_MALLOC=0 -s ASSERTIONS=0 -s DISABLE_EXCEPTION_CATCHING=1 -s EVAL_CTORS=1 -s SUPPORT_ERRNO=0 --closure 1
  set OPT_FLAGS=%OPT_FLAGS% -O3 
)

set SRC=
for %%f in (*.cpp) do call set SRC=%%SRC%%%%f 

set OUT=out/index
if not exist out mkdir out

rem Grab the Binaryen path from the ".emscripten" file (which needs to have
rem been set). We then swap the Unix-style slashes.
rem 
for /f "tokens=*" %%t in ('em-config BINARYEN_ROOT') do (set BINARYEN_ROOT=%%t)
set "BINARYEN_ROOT=%BINARYEN_ROOT:/=\%"

%SystemRoot%\system32\cmd /c "em++ %CPP_FLAGS% %OPT_FLAGS% %EMS_FLAGS% %SRC% -o %OUT%.html"
set EMCC_ERR=%errorlevel%
if %DEBUG%==false (
  if %EMCC_ERR%==0 (
    %SystemRoot%\system32\cmd /c "%BINARYEN_ROOT%\bin\wasm-opt %OPT_FLAGS% --converge %OUT%.wasm -o %OUT%.wasm"
    set EMCC_ERR=%errorlevel%
  )
)

if %EMCC_ERR%==0 (
  echo Success!
)
exit /b %EMCC_ERR%
