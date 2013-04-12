all: filesys
filesys: filesys.cpp disk.cpp
	g++ -g -o filesys filesys.cpp disk.cpp
	rm -f DISK
clean:
	rm -f *.o filesys
	rm -f DISK
