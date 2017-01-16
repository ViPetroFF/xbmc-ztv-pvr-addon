@echo off
set Playerpath=C:\Program Files\MPC-HC\mpc-hc.exe

set url=%1
set url=%url:"=%
set url=%url:~-8%

:loop
set ch=%url:~0,1%
set url=%url:~1%
if not %ch%==_ goto loop

set url=%url:~0,-4%
set /a id=url
rem set /a id+=1
rem set url=udp://@235.10.10.%id%:1234
rem set url=http://192.168.0.1:7781/udp/235.10.10.%id%:1234
set url=http://192.168.0.1:7781/%id%.ts
rem pause
rem echo "%url%" > C:\Users\Viktor\AppData\Roaming\Kodi\url.txt
"%Playerpath%" %url% /fullscreen /close
