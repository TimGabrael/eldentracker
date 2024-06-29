#include "settings.h"
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
            for(const auto& obj : settings_json) {
                // TODO: LOAD HOSTS AND WEBSOCKETS
                const auto& hosts = obj.find("hosts");
                if(hosts != obj.end() && hosts->is_array()) {
                    for(const auto& host : *hosts) {
                        const auto& ip = host.find("ip");
                    }
                }
                const auto& websockets = obj.find("websocket_hosts");
                if(websockets != obj.end() && websockets->is_array()) {
                    for(const auto& websocket : *websockets) {
                        if(websocket.is_string()) {
                            settings.websocket_connections.push_back(websocket);
                        }
                    }
                }
                const auto& listen_port = obj.find("listen_port");
                if(listen_port != obj.end() && listen_port->is_number()) {
                    settings.listen_port = *listen_port;
                }

                const auto& identifier = obj.find("identifier");
                if(identifier != obj.end() && identifier->is_string()) {
                    settings.identifier = *identifier;
                }
                const auto& log_level = obj.find("log_level");
                if(log_level != obj.end() && log_level->is_number()) {
                    SetLogLevel(*log_level);
                }
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
        try {
            nlohmann::json settings_data = 
            {
                nlohmann::json::object({{"listen_port", 6252}}),
                nlohmann::json::object({{
                    "websocket_hosts", nlohmann::json::array({
                            "ws://localhost:6251",
                            })
                }}),
                nlohmann::json::object({{ "identifier", this->identifier }}),
                nlohmann::json::object({{ "log_level", LOG_LEVEL::LOG_INFO }}),
            };
            settings_file << settings_data.dump(4);
        }
        catch(nlohmann::json::exception& e) {
            Log(string_format("failed to create settings_data: %s", e.what()), LOG_LEVEL::LOG_ERROR);
        }
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

