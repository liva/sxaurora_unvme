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

uio:
	echo '0000:b3:00.0' | sudo tee /sys/bus/pci/drivers/nvme/unbind
	echo '0000:b3:00.0' | sudo tee /sys/bus/pci/drivers/uio_pci_generic/bind

nvme:
	echo '0000:b3:00.0' | sudo tee /sys/bus/pci/drivers/uio_pci_generic/unbind
	echo '0000:b3:00.0' | sudo tee /sys/bus/pci/drivers/nvme/bind

od_nvme:
	sudo od -N32 -Ad -tx /dev/nvme0n1