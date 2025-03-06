# DupSieveNet: Distributed Image Deduplication System

## Overview

DupSieveNet is a distributed system that removes duplicate images over a network. It uses a Python script to create PNG images with random file names and some duplicates mixed in. A C++ client and server work together using sockets over TCP/IP. The server decodes the PNG images with OpenCV, extracts the raw pixel data, and then computes an MD5 hash (using the OpenSSL EVP API) to check for duplicates. Only unique images are saved. Both the client and server record and display performance stats like how many images were processed, how many duplicates were found, and the processing time.

All important settings (like server IP, port, folders, image dimensions, etc.) are stored in a configuration file (**config.txt**) so that no sensitive data is hard-coded.

## Intuition

Think of DupSieveNet like a smart photo organizer for a large collection of images. In real life, you might have many copies of the same photo. Instead of keeping every copy, a good deduplication system stores just one copy of each unique image.

Here's how the system works:
- **Image Generation**: The Python script creates many PNG images with random names and intentionally adds duplicates. This randomness simulates how files might be saved from different sources.
- **Client-Server Flow**: The client sends each PNG file (with a small header showing its size) to the server. The server decodes the image using OpenCV to see the actual picture and then hashes the pixel data to decide if the image is new or a duplicate.
- **Real-World Match**: Just like cloud storage services that save only one copy of a repeated photo, this system keeps only the unique images. This makes it efficient and realistic.

## Motivation and Rationale

This project started as an experiment in image processing and network communication. I built it step by step into a robust deduplication system. The use of random file names ensures that duplicates are mixed throughout the folder—much like how duplicates appear in the real world. Decoding the PNG files and hashing their pixel data guarantees that only truly unique images are kept, even if their file formats or metadata differ slightly.

## Features

- **Randomized PNG Image Generation**  
  The Python script uses the Pillow library to create unique PNG images with random names. It also makes duplicates with new names so that they appear randomly in the folder.

- **Timing and Performance Metrics**  
  - The Python generator shows how long it takes to create and duplicate the images.
  - The C++ server logs the total number of images received, the number of duplicates, and the number of unique images.
  - The C++ client displays how many images were sent and the total processing time.

- **Socket Communication**  
  The client and server use POSIX sockets over TCP/IP for reliable, connection-oriented communication.

- **Image Deduplication via Decoding**  
  The server decodes PNG files using OpenCV and hashes the raw pixel data. This method ensures that images are compared based on visual content rather than just file data.

- **Configuration File**  
  All important variables (like server IP, port, folder paths, image dimensions, etc.) are stored in a **config.txt** file. This helps keep sensitive data out of the code.

## Technical Details

### Image Specifications

- **Resolution**: 800 x 600 pixels  
- **Color Channels**: 3 (RGB)  
- **File Format**: PNG

### Python Image Generator

- **Random Naming**:  
  Uses UUIDs to generate random file names so that both unique images and duplicates are mixed together.
- **Timing**:  
  Uses Python’s `time` module to record and display the generation time.
- **Output Format**:  
  Images are saved as PNG files.
- **Library**:  
  Uses Pillow to create and save images.

### Server (C++)

- **Socket Programming**:  
  The server creates a TCP socket, binds to a port (read from **config.txt**), and listens for client connections.
- **Image Decoding & Deduplication**:  
  It receives each PNG file with a 4-byte header (indicating file size) followed by the file data. The server decodes the PNG using OpenCV, verifies its dimensions and channels, and computes an MD5 hash on the raw pixel data.
- **Logging and Storage**:  
  The server prints stats (total images, duplicates, and unique images) and saves unique images in the folder specified in the config file.
- **Configuration**:  
  Reads settings like port and storage folder from **config.txt**.

### Client (C++)

- **Socket Communication**:  
  The client connects to the server using the IP address and port from **config.txt**.
- **File Transfer**:  
  It scans the image folder (from **config.txt**) for PNG files. For each file, it sends a 4-byte header (file size) followed by the file data.
- **Performance Metrics**:  
  Uses C++’s `<chrono>` library to measure the time taken to send the images and displays the total number sent.

## How to Run

### Prerequisites

- **Python 3.x**  
- **C++ Compiler** (e.g., g++ or clang++)  
- **OpenSSL Development Libraries** (for the C++ server code)  
- **OpenCV** (for decoding PNG images in C++)  
- **POSIX-compliant system** (Linux or Termux for Android)

### Steps

1. **Clone the Repository**  
   ```bash
   git clone https://github.com/abdullah-a8/DupSieveNet.git
   cd DupSieveNet
   ```

2. **Set Up the Configuration**  
   Edit the **config.txt** file if needed. This file holds all settings like `SERVER_IP`, `SERVER_PORT`, folder paths, and image dimensions.

3. **Generate PNG Images**  
   Run the Python script to create images with random names and duplicates:  
   ```bash
   python3 image_generator.py
   ```  
   The script prints the time taken to generate and duplicate the images.

### Building the Project with Makefile

A Makefile is provided to compile both the server and client executables. Below is the Makefile content:

```makefile
CC = g++
CFLAGS = -Wall -std=c++17 $(shell pkg-config --cflags opencv4)
LDFLAGS = -lssl -lcrypto $(shell pkg-config --libs opencv4)

# Define executable names
SERVER = server_dedup
CLIENT = client_dedup

# Define source files (add more source files if necessary)
SRCS_SERVER = server_dedup.cpp other_server_file.cpp
SRCS_CLIENT = client_dedup.cpp other_client_file.cpp

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
```

**How to Use the Makefile:**

1. **Navigate to your project directory:**
   ```bash
   cd /path/to/your/project
   ```

2. **Compile both server and client:**
   ```bash
   make
   ```

3. **Run the Server First:**
   ```bash
   ./server_dedup
   ```

4. **Run the Client After the Server:**
   ```bash
   ./client_dedup
   ```

5. **Clean up compiled files:**
   ```bash
   make clean
   ```

## Design Considerations

- **Random Naming for Realism**:  
  Random file names mix the duplicate images, which tests the system under realistic conditions.

- **Performance and Scalability**:  
  The system uses timers and counters to track performance. Its modular design makes it easier to scale or extend (for example, handling multiple clients).

- **Distributed Processing**:  
  The project shows how simple image processing tasks can be offloaded to a server. This is a basic example of distributed computing in action.

## Future Enhancements

- **Multi-Client Support and Concurrency**:  
  Add support for handling multiple clients concurrently using multithreading, asynchronous I/O, or event-driven frameworks. This will help improve throughput and reduce latency.

- **Batching and Network Optimizations**:  
  Consider batching multiple images into a single transmission to reduce socket call overhead. Additionally, explore network-level optimizations (e.g., adjusting buffer sizes, using TCP_NODELAY) for lower latency.

- **Advanced Image Processing and Perceptual Hashing**:  
  Use more advanced image processing libraries to enhance image analysis. Investigate perceptual hashing techniques to identify near-duplicate images, balancing between improved accuracy and processing time.

- **Better Error Handling and Resource Management**:  
  Enhance error checking and recovery in both the client and server. Optimize resource management by caching or reusing objects (e.g., for OpenCV decoding) to reduce memory allocation overhead.

- **User Interface and Data Analytics**:  
  Build a simple GUI or web interface for easier system monitoring. Integrate logging frameworks or databases for detailed performance tracking and analytics.

- **Profiling and Performance Benchmarking**:  
  Implement profiling and benchmarking tools to identify bottlenecks and further fine-tune system performance.


## Final Thoughts
DupSieveNet is a hands-on project that shows how distributed computing and network programming can solve real-world problems like image deduplication. With its modular design, clear performance metrics, and robust deduplication logic using PNG decoding and pixel-level hashing, it is a solid foundation for further study and development in distributed image processing systems.

---

### DAA - Assignment 5B - BSCS23109