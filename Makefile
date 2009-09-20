
all:	config_manager

config_manager:	config_manager.cc config_obj.o
	g++ -g -lxerces-c -o config_manager config_obj.o config_manager.cc

config_obj.o:	config_obj.cc config_obj.hh
	g++ -g -c -o config_obj.o config_obj.cc

clean:
	rm -f config_manager *.o myconf_new.conf

run:
	./config_manager
