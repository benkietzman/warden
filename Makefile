###########################################
# Warden
# -------------------------------------
# file       : Makefile
# author     : Ben Kietzman
# begin      : 2021-04-06
# copyright  : kietzman.org
# email      : ben@kietzman.org
###########################################

prefix=/usr/local

all: bin/vault bin/warden

bin/vault: ../common/libcommon.a obj/vault.o
	-if [ ! -d bin ]; then mkdir bin; fi;
	g++ -o bin/vault obj/vault.o $(LDFLAGS) -L../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lnsl -lpthread -lrt -lssl -ltar -lz

bin/warden: ../common/libcommon.a obj/warden.o
	-if [ ! -d bin ]; then mkdir bin; fi;
	g++ -o bin/warden obj/warden.o $(LDFLAGS) -L../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lnsl -lpthread -lrt -lssl -ltar -lz

../common/libcommon.a: ../common/Makefile
	cd ../common; ./configure; make;

../common/Makefile: ../common/configure
	cd ../common; ./configure;

../common/configure:
	cd ../; git clone https://github.com/benkietzman/common.git

obj/vault.o: vault.cpp ../common/Makefile
	-if [ ! -d obj ]; then mkdir obj; fi;
	g++ -g -Wall -c vault.cpp -o obj/vault.o $(CPPFLAGS) -I../common

obj/warden.o: warden.cpp ../common/Makefile
	-if [ ! -d obj ]; then mkdir obj; fi;
	g++ -g -Wall -c warden.cpp -o obj/warden.o $(CPPFLAGS) -I../common

install: bin/vault bin/warden
	-if [ ! -d $(prefix)/warden ]; then mkdir $(prefix)/warden; fi;
	-if [ ! -d $(prefix)/warden/module ]; then mkdir $(prefix)/warden/module; fi;
	install --mode=777 bin/vault $(prefix)/warden/vault
	install --mode=777 bin/warden $(prefix)/warden/warden_preload
	if [ ! -f /lib/systemd/system/warden.service ]; then install --mode=644 warden.service /lib/systemd/system/; fi

clean:
	-rm -fr obj bin

uninstall:
	-rm -fr /usr/local/warden
