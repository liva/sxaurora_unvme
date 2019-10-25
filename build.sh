#!/bin/sh -xe
docker rm -f unvme || :
docker run --rm -it -v $PWD:$PWD -w $PWD veos:develop rm -f src/libunvme.a
docker run --rm -it -v $PWD:$PWD -w $PWD veos:develop make -C src
docker run -d --name unvme -it -v $PWD:$PWD -w $PWD veos:develop sh
#docker exec -it unvme mkdir -p workdir
#docker exec -it unvme mkdir -p workdir/unvme
#docker exec -it -w $PWD/workdir/unvme unvme /opt/nec/nosupport/llvm-ve/bin/llvm-ar -x ../../src/libunvme.a
#docker exec -it unvme mkdir -p workdir/sysve
#docker exec -it -w $PWD/workdir/sysve unvme /opt/nec/nosupport/llvm-ve/bin/llvm-ar -x /opt/nec/ve/lib/libsysve.a
#docker exec -it -w $PWD/workdir unvme sh -c '/opt/nec/nosupport/llvm-ve/bin/llvm-ar rcs ../libvedio.a unvme/*.o sysve/*.o'
#docker exec -it unvme rm -r workdir || :
docker exec -it unvme mkdir -p /opt/nec/ve/ex_include/
docker exec -it unvme mkdir -p /opt/nec/ve/ex_lib/
docker exec -it unvme cp src/libunvme.a /opt/nec/ve/lib/
docker exec -it unvme cp src/libunvme.a /opt/nec/ve/ex_lib/
docker exec -it unvme sh -c 'cp src/*.h /opt/nec/ve/include/'
docker exec -it unvme sh -c 'cp src/*.h /opt/nec/ve/ex_include/'
docker commit unvme unvme:ve
docker rm -f unvme

