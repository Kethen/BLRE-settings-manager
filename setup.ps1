$moduleName=$args[0]
# rename files and directories
Rename-Item ./include/skeleton/skeleton.h -NewName "$moduleName.h"
Rename-Item -Path ./include/skeleton/ -NewName $moduleName
Rename-Item ./source/skeleton.cpp -NewName "$moduleName.cpp"
Rename-Item ./source/Skeleton.rc -NewName "$moduleName.rc"

# rename project name in source files
(Get-Content source/module.json).replace('Skeleton', $args[0]) | Set-Content source/module.json
(Get-Content CMakelists.txt).replace('Skeleton', $args[0]) | Set-Content CMakelists.txt

# delete setup script
Remove-Item $PSCommandPath -Force
