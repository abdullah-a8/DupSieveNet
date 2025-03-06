//server_dedup.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_set>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <arpa/inet.h>

// OpenCV for decoding and writing PNG images
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core.hpp>

using namespace std;
using namespace cv;

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
int createServerSocket() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    return server_fd;
}

void bindAndListen(int server_fd, int port) {
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    cout << "Server listening on port " << port << endl;
}

int acceptClient(int server_fd) {
    sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int client_sock = accept(server_fd, (sockaddr*)&client_addr, &addrlen);
    if (client_sock < 0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }
    cout << "Client connected." << endl;
    return client_sock;
}

// Receives exactly 'size' bytes from the socket into buffer
bool receiveAll(int sock, unsigned char* buffer, size_t size) {
    size_t total_received = 0;
    while (total_received < size) {
        ssize_t bytes = recv(sock, buffer + total_received, size - total_received, 0);
        if (bytes <= 0) {
            return false;
        }
        total_received += bytes;
    }
    return true;
}

// Send acknowledgment back to client
bool sendAck(int sock, const string &msg) {
    ssize_t sent = send(sock, msg.c_str(), msg.size(), 0);
    return (sent == (ssize_t)msg.size());
}

int main() {
    // Load configuration
    auto config = loadConfig("config.txt");
    int SERVER_PORT = stoi(config["SERVER_PORT"]);
    string STORAGE_FOLDER = config["STORAGE_FOLDER"];
    int IMAGE_WIDTH = stoi(config["IMAGE_WIDTH"]);
    int IMAGE_HEIGHT = stoi(config["IMAGE_HEIGHT"]);
    int CHANNELS = stoi(config["CHANNELS"]);
    
    // Create storage folder if it doesn't exist
    string mkdirCmd = "mkdir -p " + STORAGE_FOLDER;
    system(mkdirCmd.c_str());

    // Create and configure the server socket
    int server_fd = createServerSocket();
    bindAndListen(server_fd, SERVER_PORT);

    unordered_set<string> knownHashes;
    int fileCounter = 0;

    while (true) {
        int client_sock = acceptClient(server_fd);
        int totalImages = 0;
        int duplicateCount = 0;

        while (true) {
            // --- Receive 4-byte header for PNG file size ---
            uint32_t netFileSize;
            if (!receiveAll(client_sock, reinterpret_cast<unsigned char*>(&netFileSize), sizeof(netFileSize))) {
                cout << "Client disconnected or no more data." << endl;
                break;
            }
            uint32_t fileSize = ntohl(netFileSize);

            // --- Receive PNG file data ---
            vector<unsigned char> pngData(fileSize);
            if (!receiveAll(client_sock, pngData.data(), fileSize)) {
                cout << "Failed to receive complete PNG data." << endl;
                break;
            }
            totalImages++;

            // --- Decode PNG using OpenCV ---
            Mat img = imdecode(pngData, IMREAD_COLOR);
            if (img.empty()) {
                cerr << "Failed to decode PNG image." << endl;
                sendAck(client_sock, "ERROR");
                continue;
            }
            if (img.cols != IMAGE_WIDTH || img.rows != IMAGE_HEIGHT || img.channels() != CHANNELS) {
                cerr << "Image dimensions or channels do not match expected values." << endl;
                sendAck(client_sock, "ERROR");
                continue;
            }
            if (!img.isContinuous())
                img = img.clone();
            
            // --- Compute MD5 hash on raw pixel data ---
            unsigned char md_value[EVP_MAX_MD_SIZE];
            unsigned int md_len = 0;
            EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
            if (!mdctx) {
                cerr << "Failed to create EVP_MD_CTX" << endl;
                exit(EXIT_FAILURE);
            }
            if (EVP_DigestInit_ex(mdctx, EVP_md5(), nullptr) != 1) {
                cerr << "EVP_DigestInit_ex failed" << endl;
                EVP_MD_CTX_free(mdctx);
                exit(EXIT_FAILURE);
            }
            size_t dataSize = img.total() * img.elemSize();
            if (EVP_DigestUpdate(mdctx, img.data, dataSize) != 1) {
                cerr << "EVP_DigestUpdate failed" << endl;
                EVP_MD_CTX_free(mdctx);
                exit(EXIT_FAILURE);
            }
            if (EVP_DigestFinal_ex(mdctx, md_value, &md_len) != 1) {
                cerr << "EVP_DigestFinal_ex failed" << endl;
                EVP_MD_CTX_free(mdctx);
                exit(EXIT_FAILURE);
            }
            EVP_MD_CTX_free(mdctx);

            char md5_string[EVP_MAX_MD_SIZE * 2 + 1];
            for (unsigned int i = 0; i < md_len; i++) {
                sprintf(&md5_string[i * 2], "%02x", md_value[i]);
            }
            md5_string[md_len * 2] = '\0';
            string hash_str(md5_string);

            // --- Deduplication Check ---
            if (knownHashes.find(hash_str) != knownHashes.end()) {
                cout << "Received a duplicate image (MD5=" << hash_str << "). Discarding." << endl;
                duplicateCount++;
                sendAck(client_sock, "DUPLICATE");
            } else {
                knownHashes.insert(hash_str);
                string filename = STORAGE_FOLDER + "unique_image_" + to_string(fileCounter++) + ".png";
                if (!imwrite(filename, img)) {
                    cerr << "Failed to save image to " << filename << endl;
                    sendAck(client_sock, "ERROR");
                    continue;
                }
                cout << "Received a new image, saved as " << filename 
                     << ", hash=" << hash_str << endl;
                sendAck(client_sock, "OK");
            }
        }

        cout << "Connection closed. Total images received: " << totalImages
             << " (Duplicates: " << duplicateCount 
             << ", Unique: " << (totalImages - duplicateCount) << ")." << endl;
        close(client_sock);
    }

    close(server_fd);
    return 0;
}