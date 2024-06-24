TARGET=MakeData
CC=g++
CFLAGS=

LDFLAGS=-lsqlite3 \
	-lssl \
	-lcrypto \

SRCS=main.cpp class_database.cpp
	
OBJS=$(SRCS:.cpp=.o)

.PHONY: all
all: $(TARGET)


.PHONY: clean
clean:
	rm $(OBJS) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

.cpp.o:
	$(CC) -c $< -o $@ $(CFLAGS)
