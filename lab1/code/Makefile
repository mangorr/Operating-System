# ssh nyuID@access.cims.nyu.edu
# ssh linserv1.cims.nyu.edu
# scp Makefile nyuID@access.cims.nyu.edu:/home/nyuID/os/lab1
# tar -xzvf lab1_assign.tar.z

# Local
# ./runit.sh /Users/yachiru/Documents/nyu/class/os/lab1/lab1_assign/output /Users/yachiru/Documents/nyu/class/os/lab1/linker
# ./gradeit.sh . /Users/yachiru/Documents/nyu/class/os/lab1/lab1_assign/output

# Server 
# ./runit.sh /home/nyuID/os/lab1/lab1_assign/check_output /home/nyuID/os/lab1/linker
# ./gradeit.sh . /home/nyuID/os/lab1/lab1_assign/check_output

CFLAGS=-std=c++11
CC=g++

linker: linker.cpp
	$(CC) $(CFLAGS) linker.cpp -o linker
