#pragma once
#include <vector>
#include <string>
#include "mem.h"

struct HostInfo {
    std::string ip;
    uint32_t port = 0;
};
struct Settings {
    static Settings& Get();
    void SaveToFile() const;
    
    std::vector<HostInfo> host_list;
    std::vector<std::string> websocket_connections;
    uint32_t listen_port = 6252;
    std::string identifier = "";
};

void SetDllPath(const char* full_path);
const std::string& GetDllPath();



