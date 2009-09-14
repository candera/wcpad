
@echo off

REM %1 is configuration name
REM %2 is platform name
REM %3 is output directory
REM %4 is project directory

copy "%4..\..\lib\wcpadlib\%2\%1\wcpadlib.dll" "%3"
copy "%4..\..\lib\opencv\%1\cv110*.dll" "%3"
copy "%4..\..\lib\opencv\%1\cvaux110*.dll" "%3"
copy "%4..\..\lib\opencv\%1\cxcore110*.dll" "%3"
copy "%4..\..\lib\opencv\%1\highgui110*.dll" "%3"