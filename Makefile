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

include ./Makefile.def

default: fio

old_run: unvme
	sudo ./unvme -h -r 0 -n 16384000 --qsize 16384 b3:00.0
	sudo ./unvme -h -w 0 -n 16384000 -p 0x12345678 --qsize 16384 b3:00.0
	sudo ./unvme -h -r 0 -n 16384000 --qsize 8192 b3:00.0
	sudo ./unvme -h -w 0 -n 16384000 -p 0x12345678 --qsize 8192 b3:00.0
	sudo ./unvme -h -r 0 -n 16384000 --qsize 4096 b3:00.0
	sudo ./unvme -h -w 0 -n 16384000 -p 0x12345678 --qsize 4096 b3:00.0
	sudo ./unvme -h -r 0 -n 16384000 --qsize 2048 b3:00.0
	sudo ./unvme -h -w 0 -n 16384000 -p 0x12345678 --qsize 2048 b3:00.0
	sudo ./unvme -h -r 0 -n 16384000 --qsize 1024 b3:00.0
	sudo ./unvme -h -w 0 -n 16384000 -p 0x12345678 --qsize 1024 b3:00.0
	sudo ./unvme -h -r 0 -n 16384000 --qsize 512 b3:00.0
	sudo ./unvme -h -w 0 -n 16384000 -p 0x12345678 --qsize 512 b3:00.0
	sudo ./unvme -h -r 0 -n 16384000 --qsize 256 b3:00.0
	sudo ./unvme -h -w 0 -n 16384000 -p 0x12345678 --qsize 256 b3:00.0
	sudo ./unvme -h -r 0 -n 16384000 --qsize 128 b3:00.0
	sudo ./unvme -h -w 0 -n 16384000 -p 0x12345678 --qsize 128 b3:00.0
	sudo ./unvme -h -r 0 -n 16384000 --qsize 64 b3:00.0
	sudo ./unvme -h -w 0 -n 16384000 -p 0x12345678 --qsize 64 b3:00.0
	sudo ./unvme -h -r 0 -n 16384000 --qsize 32 b3:00.0
	sudo ./unvme -h -w 0 -n 16384000 -p 0x12345678 --qsize 32 b3:00.0
	sudo ./unvme -h -r 0 -n 16384000 --qsize 16 b3:00.0
	sudo ./unvme -h -w 0 -n 16384000 -p 0x12345678 --qsize 16 b3:00.0

.PHONY: src

src:
	make -C $@

fio: src
	make -C ioengine
	sudo test/unvme-benchmark b3:00.0

fio2: src
	make -C ioengine
	sudo /opt/nec/ve/bin/gdb --args /home/sawamoto/fio_ve/fio /home/sawamoto/vepci/test/out/unvme-randread-01-04.fio

unvme: src test/unvme/unvme_liva.c
	$(CC) $(CFLAGS) -o $@ test/unvme/unvme_liva.c src/libunvme.a -lveio -pthread

unvme2: src test/nvme/nvme_identify.c
	$(CC) $(CFLAGS) -o $@ test/nvme/nvme_identify.c src/libunvme.a -lveio -pthread

vepci: test.c
	/opt/nec/ve/bin/ncc -o $@ $^

clean:
	-rm unvme src/*.o src/*.a ioengine/*.o ioengine/unvme_fio
# echo "144d a808" | sudo tee /sys/bus/pci/drivers/uio_pci_generic/new_id
uio:
	echo '0000:b3:00.0' | sudo tee /sys/bus/pci/drivers/nvme/unbind
	echo '0000:b3:00.0' | sudo tee /sys/bus/pci/drivers/uio_pci_generic/bind

nvme:
	echo '0000:b3:00.0' | sudo tee /sys/bus/pci/drivers/uio_pci_generic/unbind
	echo '0000:b3:00.0' | sudo tee /sys/bus/pci/drivers/nvme/bind

od_nvme:
	sudo od -N32 -Ad -tx /dev/nvme0n1