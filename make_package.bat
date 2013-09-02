del /s /f /q dist
mkdir dist
copy Release\d3d9.dll dist\d3d9.dll
copy Release\xinput1_3.dll dist\xinput1_3.dll
copy icons.dds dist\icons.dds
copy digits.dds dist\digits.dds
copy input.cfg dist\input.cfg
copy HOWTO dist\HOWTO
copy LICENSE dist\LICENSE
echo | set /p=https://github.com/00kyle00/ssf4ae-io/commit/> dist\VERSION
git rev-parse HEAD >> dist\VERSION
