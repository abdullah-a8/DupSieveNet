//client_dedup.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <chrono>
#include <unordered_map>

using namespace std;
using namespace chrono;

// --- Utility: Simple Config Parser ---
unordered_map<string, string> loadConfig(const string &filename) {
    unordered_map<string, string> config;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Could not open config file: " << filename << endl;
        exit(EXIT_FAILURE);
    }
    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        istringstream iss(line);
        string key, value;
        if (getline(iss, key, '=') && getline(iss, value))
            config[key] = value;
    }
    return config;
}

// --- Networking Helper Functions ---
int connectToServer(const char* serverIP, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "Socket creation error." << endl;
        return -1;
    }
    sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, serverIP, &serv_addr.sin_addr) <= 0) {
        cerr << "Invalid server address." << endl;
        close(sock);
        return -1;
    }
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Connection failed to " << serverIP << ":" << port << endl;
        close(sock);
        return -1;
    }
    cout << "Connected to server " << serverIP << " on port " << port << endl;
    return sock;
}

bool sendAll(int sock, const unsigned char* buffer, size_t size) {
    size_t total_sent = 0;
    while (total_sent < size) {
        ssize_t sent = send(sock, buffer + total_sent, size - total_sent, 0);
        if (sent < 0)
            return false;
        total_sent += sent;
    }
    return true;
}

string receiveAck(int sock) {
    char buffer[32];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0)
        return string(buffer);
    return "";
}

int main(int argc, char* argv[]) {
    // Load configuration
    auto config = loadConfig("config.txt");
    string SERVER_IP = config["SERVER_IP"];
    int SERVER_PORT = stoi(config["SERVER_PORT"]);
    string folderPath = config["IMAGES_FOLDER"];
    
    // Open the images folder
    DIR* dir = opendir(folderPath.c_str());
    if (!dir) {
        cerr << "Failed to open folder: " << folderPath << endl;
        return -1;
    }

    // Connect to the server
    int sock = connectToServer(SERVER_IP.c_str(), SERVER_PORT);
    if (sock < 0) {
        cerr << "Could not connect to server." << endl;
        closedir(dir);
        return -1;
    }

    int imagesSent = 0;
    auto startTime = steady_clock::now();

    // Iterate over files in the folder
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR)
            continue;

        string filename = entry->d_name;
        if (filename.size() < 4 || filename.substr(filename.size()-4) != ".png")
            continue;

        string fullPath = folderPath + "/" + filename;
        ifstream file(fullPath, ios::binary | ios::ate);
        if (!file.is_open()) {
            cerr << "Failed to open file: " << fullPath << endl;
            continue;
        }
        streamsize fileSize = file.tellg();
        file.seekg(0, ios::beg);
        vector<unsigned char> buffer(fileSize);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
            cerr << "Failed to read file data: " << fullPath << endl;
            file.close();
            continue;
        }
        file.close();

        // --- Send 4-byte file size header ---
        uint32_t netFileSize = htonl(static_cast<uint32_t>(fileSize));
        if (!sendAll(sock, reinterpret_cast<unsigned char*>(&netFileSize), sizeof(netFileSize))) {
            cerr << "Failed sending file size to server." << endl;
            close(sock);
            closedir(dir);
            return -1;
        }

        // --- Send PNG file data ---
        cout << "Sending image " << filename << " (" << fileSize << " bytes)..." << endl;
        if (!sendAll(sock, buffer.data(), fileSize)) {
            cerr << "Failed sending image to server." << endl;
            close(sock);
            closedir(dir);
            return -1;
        }
        imagesSent++;

        string ack = receiveAck(sock);
        if (ack == "OK")
            cout << "Server stored image as a new file." << endl;
        else if (ack == "DUPLICATE")
            cout << "Server indicates this image is a duplicate." << endl;
        else
            cout << "No or unknown acknowledgment from server." << endl;
    }

    closedir(dir);
    close(sock);

    auto endTime = steady_clock::now();
    auto elapsedMs = duration_cast<milliseconds>(endTime - startTime).count();
    cout << "All images sent. Total images sent: " << imagesSent 
         << ". Processing time: " << elapsedMs << " milliseconds." << endl;
    return 0;
}