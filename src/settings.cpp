#include "settings.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include "logger.h"

#define SETTINGS_FILE_NAME "settings.json"
static std::string DLL_PATH = "mods/";

static Settings LoadSettings(const char* filename) {
    Settings settings = {};
    std::ifstream settings_file(DLL_PATH + SETTINGS_FILE_NAME);
    if(settings_file.is_open()) {
        try {
            nlohmann::json settings_json = nlohmann::json::parse(settings_file);

            // TODO: LOAD HOSTS AND WEBSOCKETS
            const auto& hosts = settings_json.find("hosts");
            if(hosts != settings_json.end() && hosts->is_array()) {
                for(const auto& host : *hosts) {
                    const auto& ip = host.find("ip");
                }
            }
            const auto& websockets = settings_json.find("websocket_hosts");
            if(websockets != settings_json.end() && websockets->is_array()) {
                for(const auto& websocket : *websockets) {
                    std::cout << websocket.dump() << std::endl;
                    const auto& uri = websocket.find("uri");
                    if(uri != websocket.end() && uri->is_string()) {
                        settings.websocket_connections.push_back(*uri);
                    }
                }
            }
            const auto& listen_port = settings_json.find("listen_port");
            if(listen_port != settings_json.end() && listen_port->is_number()) {
                settings.listen_port = *listen_port;
            }

            const auto& identifier = settings_json.find("identifier");
            if(identifier != settings_json.end() && identifier->is_string()) {
                settings.identifier = *identifier;
            }
            const auto& log_level = settings_json.find("log_level");
            if(log_level != settings_json.end() && log_level->is_number()) {
                SetLogLevel(*log_level);
            }
        }
        catch(...) {
            Log("Failed to load in settings file", LOG_LEVEL::LOG_WARNING);
        }
        settings_file.close();
    }
    else {
        settings.SaveToFile();
    }
    return settings;
}


Settings& Settings::Get() {
    static Settings settings = LoadSettings(SETTINGS_FILE_NAME);
    return settings;
}
void Settings::SaveToFile() const {
    std::ofstream settings_file(DLL_PATH + SETTINGS_FILE_NAME);
    if(settings_file.is_open()) {
        nlohmann::json settings_data = 
        {
            {
                "listen_port", {
                    0x6252
                },
            },
            {
                "websocket_hosts", nlohmann::json::array({
                        nlohmann::json::object({ "uri", "ws://localhost:6251" }),
                    }
                )
            },
            { "identifier", this->identifier },
            { "log_level", LOG_LEVEL::LOG_INFO },
        };
        settings_file << settings_data.dump();
    }
}
void SetDllPath(const char* full_path) {
    DLL_PATH = full_path;
    size_t idx = DLL_PATH.find_last_of("\\");
    if(idx != -1) {
        DLL_PATH = DLL_PATH.substr(0, idx + 1);
    }
    else {
        size_t idx = DLL_PATH.find_last_of("/");
        if(idx != -1) {
            DLL_PATH = DLL_PATH.substr(0, idx + 1);
        }
        else {
            // the full path was wierd for some reason,
            // better just go to the relative mods folder path and hope for the best
            DLL_PATH = "mods/";
        }
    }
}
const std::string& GetDllPath() {
    return DLL_PATH;
}

