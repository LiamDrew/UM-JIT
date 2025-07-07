ls
cd jit
ls
make clean
make
./jit ../umasm/sandmark.umz
ls
/usr/bin/time ./jit ../umasm/sandmark.umz
exit
ls
ls
cd emulator/
ls
make
ls
/usr/bin/time ./um ../umasm/sandmark.umz
clear
ls
/usr/bin/time ./um ../umasm/sandmark.umz
cd ..
ls
cd jit
ls
/usr/bin/time ./jit ../umasm/sandmark.umz
exit
