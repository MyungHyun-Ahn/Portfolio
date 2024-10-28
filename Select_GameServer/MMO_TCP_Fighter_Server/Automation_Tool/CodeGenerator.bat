start %1\tool.exe %1

set var=%1\result

for /f %%d in ('dir %var% /b /a-d') DO (
    copy %1\result\%%d %2
)