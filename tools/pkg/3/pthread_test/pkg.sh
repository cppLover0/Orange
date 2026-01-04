x86_64-orange-mlibc-g++ -o "$1/usr/bin/fdpass_test" fdpass.c -fPIC 
x86_64-orange-mlibc-g++ -o "$1/usr/bin/ucred_test" ucred.c -fPIC
x86_64-orange-mlibc-g++ -o "$1/usr/bin/signal_test" signaltest.c -fPIC
x86_64-orange-mlibc-g++ -o "$1/usr/bin/shm_test" shm_test.c -fPIC