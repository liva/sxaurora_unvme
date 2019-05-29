run: unvme
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

fio:
	make -C ioengine FIODIR=/home/sawamoto/fio

unvme: src/aurora_pci.c src/unvme_nvme.c src/unvme_vfio.c src/unvme_log.c src/unvme_core.c src/unvme.c test/unvme/unvme_liva.c
	/opt/nec/ve/LLVM-VE-RV/bin/clang -fno-slp-vectorize -mllvm -llvm-loopvec=false -Xclang -load -Xclang libRV.so -mllvm -rv --target=ve-linux --std=gnu99 -Isrc -lsysve -o $@ $^

vepci: test.c
	/opt/nec/ve/bin/ncc -o $@ $^

clean:
	-rm unvme

uio:
	echo '0000:b3:00.0' | sudo tee /sys/bus/pci/drivers/nvme/unbind
	echo '0000:b3:00.0' | sudo tee /sys/bus/pci/drivers/uio_pci_generic/bind

nvme:
	echo '0000:b3:00.0' | sudo tee /sys/bus/pci/drivers/uio_pci_generic/unbind
	echo '0000:b3:00.0' | sudo tee /sys/bus/pci/drivers/nvme/bind

od_nvme:
	sudo od -N32 -Ad -tx /dev/nvme0n1