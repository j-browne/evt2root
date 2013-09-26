CXX=/usr/bin/g++

all : evt2root

evt2root : evt2root.cpp
	$(CXX) -g -O -Wall -fPIC -pthread -c -I/soft/intel/lucid/root/5.26.00/include/root -c evt2root.cpp
	$(CXX) -O evt2root.o -L$(ROOT_PATH)/lib -lCore -lCint -lTree -lRint -lRIO -pthread -lm -ldl -rdynamic -o evt2root


clean : 
	rm -f *o
	rm -f evt2root

