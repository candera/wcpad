
@echo on

REM %1 is quoted configuration name
REM %2 is quoted platform name
REM %3 is quoted output directory
REM %4 is quoted project directory

copy "%~4..\..\lib\wcpadlib\%~2\%~1\wcpadlib.dll" %3
copy "%~4..\..\lib\opencv\%~1\cv110*.dll" %3
copy "%~4..\..\lib\opencv\%~1\cvaux110*.dll" %3"
copy "%~4..\..\lib\opencv\%~1\cxcore110*.dll" %3
copy "%~4..\..\lib\opencv\%~1\highgui110*.dll" %3