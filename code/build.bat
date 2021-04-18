@echo off

set CodeDir=..\code
set DataDir=..\data
set LibsDir=..\libs
set OutputDir=..\build_win32
set VulkanIncludeDir="C:\VulkanSDK\1.2.135.0\Include\vulkan"
set VulkanBinDir="C:\VulkanSDK\1.2.135.0\Bin"
set AssimpDir=%LibsDir%\framework_vulkan

set CommonCompilerFlags=-Od -MTd -nologo -fp:fast -fp:except- -EHsc -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -wd4310 -wd4127 -wd4201 -wd4100 -wd4189 -wd4505 -Z7 -FC
set CommonCompilerFlags=-I %VulkanIncludeDir% %CommonCompilerFlags%
set CommonCompilerFlags=-I %LibsDir% -I %AssimpDir% %CommonCompilerFlags%
REM Check the DLLs here
set CommonLinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib Winmm.lib opengl32.lib DbgHelp.lib d3d12.lib dxgi.lib d3dcompiler.lib %AssimpDir%\assimp\libs\assimp-vc142-mt.lib

IF NOT EXIST %OutputDir% mkdir %OutputDir%

pushd %OutputDir%

del *.pdb > NUL 2> NUL

REM USING GLSL IN VK USING GLSLANGVALIDATOR
call glslangValidator -DNAIVE=1 -S comp -e main -g -V -o %DataDir%\point_cloud_naive.spv %CodeDir%\point_cloud_shaders.cpp
call glslangValidator -DDEPTH_TEST=1 -S comp -e main -g -V -o %DataDir%\point_cloud_depth.spv %CodeDir%\point_cloud_shaders.cpp
call glslangValidator -DOVERLAPPING_FAST_PATH=1 --target-env spirv1.3 -S comp -e main -g -V -o %DataDir%\point_cloud_overlap_fast.spv %CodeDir%\point_cloud_shaders.cpp

call glslangValidator -DCONVERT_TO_FLOAT=1 -S comp -e main -g -V -o %DataDir%\convert_to_float.spv %CodeDir%\point_cloud_shaders.cpp

REM ASSIMP
copy %AssimpDir%\assimp\bin\assimp-vc142-mt.dll %OutputDir%\assimp-vc142-mt.dll

REM Point Cloud Converters
call cl %CommonCompilerFlags% -Fepreprocess_robotics_3d.exe %CodeDir%\preprocess_robotics_3d_main.cpp -Fmpreprocess_robotics_3d_main.map /link %CommonLinkerFlags%

REM 64-bit build
echo WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% %CodeDir%\point_cloud_demo.cpp -Fmpoint_cloud_demo.map -LD /link %CommonLinkerFlags% -incremental:no -opt:ref -PDB:point_cloud_demo_%random%.pdb -EXPORT:Init -EXPORT:Destroy -EXPORT:SwapChainChange -EXPORT:CodeReload -EXPORT:MainLoop
del lock.tmp
call cl %CommonCompilerFlags% -DFRAMEWORK_MEMORY_SIZE=GigaBytes(2) -DDLL_NAME=point_cloud_demo -Fepoint_cloud_demo.exe %LibsDir%\framework_vulkan\win32_main.cpp -Fmpoint_cloud_demo.map /link %CommonLinkerFlags%

popd
