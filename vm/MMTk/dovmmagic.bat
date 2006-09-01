@echo off


    c:\t_harmony\drlvm\trunk\build\win_ia32_msvc_debug\deploy\jre\bin\java.exe -Xbootclasspath/p:.;mmtk.jar;junit-4.1.jar -Xem jet: org.vmmagic.unboxed.AddressTest
    
    c:\t_harmony\drlvm\trunk\build\win_ia32_msvc_debug\deploy\jre\bin\java.exe -Xbootclasspath/p:.;mmtk.jar;junit-4.1.jar -Xem jet: org.vmmagic.unboxed.ExtentTest
	
    c:\t_harmony\drlvm\trunk\build\win_ia32_msvc_debug\deploy\jre\bin\java.exe -Xbootclasspath/p:.;mmtk.jar;junit-4.1.jar -Xem jet: org.vmmagic.unboxed.OffsetTest

    c:\t_harmony\drlvm\trunk\build\win_ia32_msvc_debug\deploy\jre\bin\java.exe -Xbootclasspath/p:.;mmtk.jar;junit-4.1.jar -Xem jet: org.vmmagic.unboxed.WordTest

    c:\t_harmony\drlvm\trunk\build\win_ia32_msvc_debug\deploy\jre\bin\java.exe -Xbootclasspath/p:.;mmtk.jar;junit-4.1.jar -Xem jet: org.vmmagic.unboxed.ObjectReferenceTest

