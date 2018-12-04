CC = gcc

all: oss user

oss: oss.c header.h 
	gcc -g -Wall -lpthread -lrt -lm -o oss oss.c

user: user.c header.h
	gcc -g -Wall -lpthread -lrt -lm -o user user.c

clean: 
	$(RM) oss user
