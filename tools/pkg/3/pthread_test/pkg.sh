x86_64-linux-gnu-g++ -o "$1/usr/bin/fdpass_test" fdpass.c -fPIC 
x86_64-linux-gnu-g++ -o "$1/usr/bin/ucred_test" ucred.c -fPIC
x86_64-linux-gnu-g++ -o "$1/usr/bin/signal_test" signaltest.c -fPIC
x86_64-linux-gnu-g++ -o "$1/usr/bin/shm_test" shm_test.c -fPIC
x86_64-linux-gnu-g++ -o "$1/usr/bin/print_test" print_test.c -fPIC
x86_64-linux-gnu-gcc -o "$1/usr/bin/statx_test" statx_test.c -fPIC -Wno-implicit-function-declaration
x86_64-linux-gnu-gcc -o "$1/usr/bin/alarm_test" alarm_test.c -fPIC
x86_64-linux-gnu-gcc -o "$1/usr/bin/setitimer_test" setitimer_test.c -fPIC
x86_64-linux-gnu-gcc -o "$1/usr/bin/socket_test" socket_test.c -fPIC