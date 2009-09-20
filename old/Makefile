all: example test_config

util.o: util.hh util.cc
	g++ -Wall -Werror -c -o util.o util.cc

nsconfig.o: nsconfig.hh nsconfig.cc util.o
	g++ -Wall -Werror -c -o nsconfig.o nsconfig.cc

test_config: nsconfig.o test_config.cc
	g++ -Wall -Werror -g nsconfig.o util.o test_config.cc -o test_config

example: nsconfig.o util.o example.cc
	g++ -Wall -Werror -g nsconfig.o util.o example.cc -o example

clean:
	rm -f example test_config *.o config.hh.gch
