OBJ = main.o scalerouter.o shared_memory.o
CXXFLAGS = -std=gnu++11 -O3
client: $(OBJ)
	g++ -o server $(OBJ) -libverbs -lpthread -lrt -g
	# g++ -o server $(OBJ) -lrdmacm -libverbs -lpthread -lrt -g
main.o: main.cpp scalerouter.h common.h log.h types.h shared_memory.h
	g++ -std=gnu++11 -O3   -c -o main.o main.cpp -g
scalerouter.o: scalerouter.cpp
	g++ -std=gnu++11 -O3   -c -o scalerouter.o scalerouter.cpp -g
shared_memory.o: shared_memory.cpp
	g++ -std=gnu++11 -O3   -c -o shared_memory.o shared_memory.cpp -g

clean:
	rm server *.o

