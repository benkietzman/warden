###########################################
# Warden
# -------------------------------------
# file       : Makefile
# author     : Ben Kietzman
# begin      : 2021-04-06
# copyright  : kietzman.org
# email      : ben@kietzman.org
###########################################

all: bin/warden

bin/warden: ../common/libcommon.a obj/warden.o
	-if [ ! -d bin ]; then mkdir bin; fi;
	g++ -o bin/warden obj/warden.o -L../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lnsl -lpthread -lrt -lssl -ltar -lz

../common/libcommon.a: ../common/Makefile
	cd ../common; ./configure; make;

../common/Makefile: ../common/configure
	cd ../common; ./configure;

../common/configure:
	cd ../; git clone https://github.com/benkietzman/common.git

obj/warden.o: warden.cpp ../common/Makefile
	-if [ ! -d obj ]; then mkdir obj; fi;
	g++ -g -Wall -c warden.cpp -o obj/warden.o -I../common

install: bin/warden
	-if [ ! -d /usr/local/warden ]; then mkdir /usr/local/warden; fi;
	-if [ ! -d /usr/local/warden/module ]; then mkdir /usr/local/warden/module; fi;
	install --mode=777 bin/warden /usr/local/warden/warden_preload
	if [ ! -f /lib/systemd/system/warden.service ]; then install --mode=644 warden.service /lib/systemd/system/; fi

clean:
	-rm -fr obj bin

uninstall:
	-rm -fr /usr/local/warden
