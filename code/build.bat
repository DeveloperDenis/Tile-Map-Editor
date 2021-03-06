@echo off

SET cflags=-Zi /FC -nologo /W4 /WX /wd4100 /wd4189 /wd4706 /wd4101 /wd4505 /wd4701 /wd4703 /wd4127 /wd4201

SET cfiles=..\code\main.cpp ..\code\ui_elements.cpp ..\code\file_saving_loading.cpp ..\code\denis_adt.cpp ..\code\new_tile_map_panel.cpp ..\code\tile_set_panel.cpp ..\code\tile_map_panel.cpp ..\code\import_tile_set_panel.cpp ..\code\tile_map_file.cpp

pushd ..\build
cl %cflags% %cfiles% /I C:\SDL2-2.0.4\include\ /link /LIBPATH:C:\SDL2-2.0.4\lib\x64\ SDL2.lib SDL2main.lib SDL2_ttf.lib SDL2_image.lib Comdlg32.lib /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup
popd
