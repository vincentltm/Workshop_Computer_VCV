#pragma once

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
    typedef SOCKET socket_t;
    #define close_socket closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    typedef int socket_t;
    #define close_socket close
    #define INVALID_SOCKET -1
#endif

#include <thread>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include <queue>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <atomic>
#include <jansson.h>
#include "plugin_local.hpp"
#include "pico_mocks.h"

extern std::mutex g_instances_mutex;
extern std::set<void*> g_instances;
extern int g_web_server_port;
extern std::atomic<bool> g_web_server_running;
extern socket_t g_server_fd;
extern std::thread g_server_thread;

// Outgoing message queue definition
class ThreadSafeMessageQueue {
private:
    std::mutex mutex_;
    std::queue<std::vector<uint8_t>> q_;
public:
    void push(const std::vector<uint8_t>& msg) {
        std::lock_guard<std::mutex> l(mutex_);
        q_.push(msg);
    }
    bool pop(std::vector<uint8_t>& msg) {
        std::lock_guard<std::mutex> l(mutex_);
        if (q_.empty()) return false;
        msg = q_.front();
        q_.pop();
        return true;
    }
    bool empty() {
        std::lock_guard<std::mutex> l(mutex_);
        return q_.empty();
    }
    void clear() {
        std::lock_guard<std::mutex> l(mutex_);
        while (!q_.empty()) q_.pop();
    }
};

class MidiTxParser {
private:
    std::vector<uint8_t> buffer_;
    uint8_t running_status_ = 0;
    bool in_sysex_ = false;
    size_t expected_len_ = 0;
public:
    void parse_byte(uint8_t b, std::vector<rack::midi::Message>& out_messages) {
        if (b >= 0xF8) {
            rack::midi::Message msg;
            msg.bytes.resize(1);
            msg.bytes[0] = b;
            out_messages.push_back(msg);
            return;
        }

        if (b == 0xF0) {
            in_sysex_ = true;
            buffer_.clear();
            buffer_.push_back(b);
            return;
        }

        if (in_sysex_) {
            buffer_.push_back(b);
            if (b == 0xF7) {
                rack::midi::Message msg;
                msg.bytes = buffer_;
                out_messages.push_back(msg);
                in_sysex_ = false;
                buffer_.clear();
            }
            return;
        }

        if (b & 0x80) {
            running_status_ = b;
            buffer_.clear();
            buffer_.push_back(b);
            uint8_t high_nibble = b & 0xF0;
            if (high_nibble == 0x80 || high_nibble == 0x90 || high_nibble == 0xA0 || high_nibble == 0xB0 || high_nibble == 0xE0 || b == 0xF2) {
                expected_len_ = 3;
            } else if (high_nibble == 0xC0 || high_nibble == 0xD0 || b == 0xF1 || b == 0xF3) {
                expected_len_ = 2;
            } else {
                expected_len_ = 1;
            }
            
            if (buffer_.size() == expected_len_) {
                rack::midi::Message msg;
                msg.bytes = buffer_;
                out_messages.push_back(msg);
                buffer_.clear();
            }
            return;
        }

        if (running_status_ != 0) {
            if (buffer_.empty()) {
                buffer_.push_back(running_status_);
            }
            buffer_.push_back(b);
            if (buffer_.size() == expected_len_) {
                rack::midi::Message msg;
                msg.bytes = buffer_;
                out_messages.push_back(msg);
                buffer_.clear();
            }
        }
    }
};

// Simple SHA-1 and Base64 implementations for WebSocket handshake
namespace sha1 {
    #define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

    inline void cycle(uint32_t* hash, uint32_t* buffer) {
        uint32_t a = hash[0];
        uint32_t b = hash[1];
        uint32_t c = hash[2];
        uint32_t d = hash[3];
        uint32_t e = hash[4];

        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = rol(a, 5) + f + e + k + buffer[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = temp;
        }

        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
    }

    inline void calculate(const std::string& input, uint8_t binary_out[20]) {
        uint32_t hash[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
        uint64_t bit_length = input.length() * 8;
        std::string padded = input;
        padded.push_back((char)0x80);
        while ((padded.length() * 8) % 512 != 448) {
            padded.push_back(0x00);
        }
        for (int i = 7; i >= 0; i--) {
            padded.push_back((char)((bit_length >> (i * 8)) & 0xFF));
        }

        uint32_t buffer[80];
        for (size_t offset = 0; offset < padded.length(); offset += 64) {
            for (int i = 0; i < 16; i++) {
                buffer[i] = ((uint8_t)padded[offset + i * 4] << 24) |
                            ((uint8_t)padded[offset + i * 4 + 1] << 16) |
                            ((uint8_t)padded[offset + i * 4 + 2] << 8) |
                            ((uint8_t)padded[offset + i * 4 + 3]);
            }
            for (int i = 16; i < 80; i++) {
                buffer[i] = rol(buffer[i - 3] ^ buffer[i - 8] ^ buffer[i - 14] ^ buffer[i - 16], 1);
            }
            cycle(hash, buffer);
        }

        for (int i = 0; i < 5; i++) {
            binary_out[i * 4]     = (hash[i] >> 24) & 0xFF;
            binary_out[i * 4 + 1] = (hash[i] >> 16) & 0xFF;
            binary_out[i * 4 + 2] = (hash[i] >> 8) & 0xFF;
            binary_out[i * 4 + 3] = hash[i] & 0xFF;
        }
    }
    #undef rol
}

namespace base64 {
    inline std::string encode(const uint8_t* data, size_t len) {
        static const char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((len + 2) / 3) * 4);
        int val = 0, valb = -6;
        for (size_t i = 0; i < len; i++) {
            uint8_t c = data[i];
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                out.push_back(lookup[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) out.push_back(lookup[((val << 8) >> (valb + 8)) & 0x3F]);
        while (out.size() % 4) out.push_back('=');
        return out;
    }
}

// Socket helpers
inline bool recv_all(socket_t fd, uint8_t* buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        int r = recv(fd, (char*)buf + total, len - total, 0);
        if (r <= 0) return false;
        total += r;
    }
    return true;
}

inline bool is_readable(socket_t fd, int ms) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    int r = select((int)fd + 1, &fds, nullptr, nullptr, &tv);
    return r > 0;
}

inline void set_socket_timeout(socket_t fd, int ms) {
#ifdef _WIN32
    DWORD timeout = ms;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

inline std::string get_mime_type(const std::string& path) {
    if (path.find(".html") != std::string::npos || path.find(".htm") != std::string::npos) return "text/html; charset=utf-8";
    if (path.find(".js") != std::string::npos) return "application/javascript; charset=utf-8";
    if (path.find(".css") != std::string::npos) return "text/css; charset=utf-8";
    if (path.find(".png") != std::string::npos) return "image/png";
    if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos) return "image/jpeg";
    if (path.find(".json") != std::string::npos) return "application/json";
    return "application/octet-stream";
}

inline void send_websocket_frame(socket_t fd, uint8_t opcode, const std::vector<uint8_t>& data, const std::string& type) {
    std::string json = "{\"type\":\"" + type + "\",\"data\":[";
    for (size_t i = 0; i < data.size(); i++) {
        json += std::to_string(data[i]);
        if (i + 1 < data.size()) json += ",";
    }
    json += "]}";

    std::vector<uint8_t> frame;
    frame.push_back(0x80 | opcode); // Fin + Opcode

    size_t len = json.length();
    if (len < 126) {
        frame.push_back((uint8_t)len);
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back((uint8_t)((len >> 8) & 0xFF));
        frame.push_back((uint8_t)(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back((uint8_t)((len >> (i * 8)) & 0xFF));
        }
    }

    frame.insert(frame.end(), json.begin(), json.end());
    send(fd, (const char*)frame.data(), frame.size(), 0);
}

inline std::string get_header_value(const std::string& request, const std::string& header_name) {
    size_t pos = request.find(header_name + ":");
    if (pos == std::string::npos) return "";
    size_t val_start = pos + header_name.length() + 1;
    while (val_start < request.length() && (request[val_start] == ' ' || request[val_start] == '\t')) {
        val_start++;
    }
    size_t val_end = request.find("\r\n", val_start);
    if (val_end == std::string::npos) val_end = request.length();
    return request.substr(val_start, val_end - val_start);
}

// Forward declare the MIDI queue push function from WorkshopComputer.cpp
void push_midi_to_rx_queue(const uint8_t* msg_bytes, size_t msg_size);

// Full implementation of handle_client and the server loop
void handle_client(socket_t client_fd);

inline void server_loop() {
    while (g_web_server_running) {
        if (is_readable(g_server_fd, 500)) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            socket_t client_fd = accept(g_server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd != INVALID_SOCKET) {
                std::thread(handle_client, client_fd).detach();
            }
        }
    }
}

inline void start_web_server() {
    if (g_web_server_running) return;

#ifdef _WIN32
    static bool winsock_initialized = false;
    if (!winsock_initialized) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2,2), &wsaData);
        winsock_initialized = true;
    }
#endif

    socket_t server_fd = INVALID_SOCKET;
    int port = 8000;
    for (; port <= 8999; port++) {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == INVALID_SOCKET) continue;

        int opt = 1;
#ifdef _WIN32
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = inet_addr("127.0.0.1"); // Only bind local loopback for security
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == 0) {
            break;
        }
        close_socket(server_fd);
        server_fd = INVALID_SOCKET;
    }

    if (server_fd == INVALID_SOCKET) {
        DEBUG("Web Server failed to bind to any port in 8000-8999");
        return;
    }

    if (listen(server_fd, 10) < 0) {
        close_socket(server_fd);
        DEBUG("Web Server failed to listen");
        return;
    }

    g_server_fd = server_fd;
    g_web_server_port = port;
    g_web_server_running = true;

    g_server_thread = std::thread(server_loop);
    DEBUG("Web Server started on port %d", port);
}

inline void stop_web_server() {
    if (!g_web_server_running) return;
    g_web_server_running = false;

    if (g_server_fd != INVALID_SOCKET) {
        close_socket(g_server_fd);
        g_server_fd = INVALID_SOCKET;
    }

    if (g_server_thread.joinable()) {
        g_server_thread.join();
    }
}
