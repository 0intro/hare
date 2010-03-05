#!/dis/sh
load std
bind  '#|' /tmp/pipes
cat /tmp/pipes/data > /dev/hoststdout&
cat /dev/hoststdin > /tmp/pipes/data&
/dis/export /csrv <>/tmp/pipes/data1 >[2] /dev/null
/dis/echo halt > /dev/sysctl
