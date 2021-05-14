#/bin/sh

cd qemu-4.1.1
./configure --target-list=x86_64-softmmu --enable-debug
make -j8
make install
cd ..

