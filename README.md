
# NVD RunAsService
(C) 2022, Nikolay Dudkin

Run application as a service.\
This application installs itself as a service which launches an executable of your choice at service start and, optionally, checks for the executable process presence, restarting it, if it is missing.

Usage:\
**ras.exe\
-servicename:\"service name\"**\
Name to be used for the service with service manager.\
**[-install**\
Install and confgure new service.\
**&nbsp;&nbsp;&nbsp;&nbsp;-path:\"path to executable\"**\
&nbsp;&nbsp;&nbsp;&nbsp;Path to actual payload executable, obligatory for installation.\
**&nbsp;&nbsp;&nbsp;&nbsp;[-setdirectory:\"path to working directory\"]**\
&nbsp;&nbsp;&nbsp;&nbsp;Path to a working directory for the executable, optional.\
**&nbsp;&nbsp;&nbsp;&nbsp;[-starttype:<auto|demand|disabled>]**\
&nbsp;&nbsp;&nbsp;&nbsp;Service startup type, optional, auto is used by default.\
**&nbsp;&nbsp;&nbsp;&nbsp;[-account:\"account name\" -password:\"account password\"]**\
&nbsp;&nbsp;&nbsp;&nbsp;User account named and password for the service to be run under, optional, LocalSystem is used by default.\
**&nbsp;&nbsp;&nbsp;&nbsp;[-loop:M]**\
&nbsp;&nbsp;&nbsp;&nbsp;How often to check that the payload executable process is running and restart, if it is not. In minutes, optional, 0 (run once and do not check) is used by default\
**&nbsp;&nbsp;&nbsp;&nbsp;[-delay:M]]**\
&nbsp;&nbsp;&nbsp;&nbsp;How long to wait after service has been started before launching payload executable. In minutes, optional, 0 (start immediately) is used by default.\
**[-uninstall]**\
Uninstall existing service.\
**[-start]**\
Start service.\
**[-stop]**\
Stop service.\
**[-silent]**\
Do not display any messages.\
\
Example:\
C:\Program Files\MyApp\ras.exe -servicename:MyService -install -path:"C:\Program Files\MyApp\MyApp.exe" -setdirectory:"C:\Program Files\MyApp\\" -starttype:auto -account:".\user1" -password:"password1" -loop:30 -delay:2 -start
