CC = g++
CFLAGS = -Wall -std=c++17 $(shell pkg-config --cflags opencv4)
LDFLAGS = -lssl -lcrypto $(shell pkg-config --libs opencv4)

# Define executable names
SERVER = server_dedup
CLIENT = client_dedup

# Define source files (add more source files if necessary)
SRCS_SERVER = server_dedup.cpp
SRCS_CLIENT = client_dedup.cpp

# Define object files
OBJS_SERVER = $(SRCS_SERVER:.cpp=.o)
OBJS_CLIENT = $(SRCS_CLIENT:.cpp=.o)

# Default target (builds both server and client)
all: $(SERVER) $(CLIENT)

# Build the server executable
$(SERVER): $(OBJS_SERVER)
	$(CC) $(CFLAGS) -o $(SERVER) $(OBJS_SERVER) $(LDFLAGS)

# Build the client executable
$(CLIENT): $(OBJS_CLIENT)
	$(CC) $(CFLAGS) -o $(CLIENT) $(OBJS_CLIENT) $(LDFLAGS)

# Compile C++ source files into object files
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Clean compiled files
clean:
	rm -f $(SERVER) $(CLIENT) *.o