#
# Copyright 2020 NEC Laboratories Europe GmbH
# All rights reserved
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#    1. Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#    2. Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
# 
#    3. Neither the name of NEC Laboratories Europe GmbH nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY NEC Laboratories Europe GmbH AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NEC Laboratories 
# Europe GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#!/bin/sh -xe
docker build docker -t build-base:develop
docker rm -f unvme || :
docker run --rm -it -v $PWD:$PWD -w $PWD build-base:develop rm -f src/libunvme.a
docker run --rm -it -v $PWD:$PWD -w $PWD build-base:develop make -C src
docker run -d --name unvme -it -v $PWD:$PWD -w $PWD build-base:develop sh
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

