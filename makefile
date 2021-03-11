CC      = /usr/bin/gcc
CFLAGS  = -Wall -g -D_REENTRANT -DDEBUG
LDFLAGS = -lwiringPi


OBJ = sd_test.o

sd_test: $(OBJ)
	$(CC) $(CFLAGS) -o sd_test $(OBJ) $(LDFLAGS)

%.o: ../%.c ../%.h
	$(CC) $(CFLAGS) -c $<

