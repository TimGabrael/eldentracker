#pragma once
#include "settings.h"
#include <string>



void Net_StartListening(uint32_t port);
void Net_ConnectToWebsocket(const std::string& uri);

void Net_SendData(const std::string& json_str);

