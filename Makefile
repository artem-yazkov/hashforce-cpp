CC      = g++
CFLAGS  = -Wall -std=c++11
LDFLAGS = -lpthread
TARGET  = hashforce

all:
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) hashforce.cpp md5.cpp

clean:
	$(RM) $(TARGET)
