ls
cd jit
ls
make clean
make
./jit ../umasm/sandmark.umz
ls
/usr/bin/time ./jit ../umasm/sandmark.umz
exit
