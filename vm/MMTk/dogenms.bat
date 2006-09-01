@echo off


c:\t_harmony\drlvm\trunk\build\win_ia32_msvc_debug\deploy\jre\bin\java.exe -Xbootclasspath/p:.;mmtk.jar -Xem jet: -Xjit jet::wb4j -Xms512m -Xms512m TestGenMS %1 
