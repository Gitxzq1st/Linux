.PHONY:all
all:tcpserver client
tcpserver:main.cc
	g++ -o $@ $^ -std=c++11 -ljsoncpp
client:ClientCal.cc
	g++ -o $@ $^ -std=c++11 -ljsoncpp

.PHONY:clean
clean:
	rm -f tcpserver client
