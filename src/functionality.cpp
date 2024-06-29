#include "functionality.h"
#include <iostream>
#include "settings.h"
#include "logger.h"
#include "networking.h"
#define NOMINMAX
#include <windows.h>

struct EntityMessage {
    uint64_t handle;
    Vector3 pos;
    Vector2 map_pos;
    Vector2 normalized_map_pos;
    uint32_t hp;
    uint32_t max_hp;
    uint32_t npc_id;
    bool is_player;
    nlohmann::json ToJson() const {
        return {
            {"pos", {this->pos.x, this->pos.y, this->pos.z}},
            {"map_pos", {this->map_pos.x, this->map_pos.y}},
            {"normalized_map_pos", {this->normalized_map_pos.x, this->normalized_map_pos.y}},
            {"hp", this->hp},
            {"max_hp", this->max_hp},
            {"npc_id", this->npc_id},
            {"is_player", this->is_player},
            {"handle", this->handle},
        };
    }
};
struct DamageMessage {
    EntityMessage sender;
    EntityMessage receiver;
    uint32_t damage_value;
    nlohmann::json ToJson() const {
        return { 
            {
                "damage", {
                    {"sender", this->sender.ToJson()},
                    {"receiver", this->receiver.ToJson()},
                    {"damage_value", this->damage_value}
                }
            }
        };
    }
};
struct DeathMessage {
    EntityMessage player;
    uint32_t death_count;
    nlohmann::json ToJson() const {
        return {
            {
                "death", {
                    {"player", this->player.ToJson()},
                    {"death_count", this->death_count},
                }
            }
        };
    }
};
struct InfoMessage {
    EntityMessage player;
    uint32_t death_count;
    nlohmann::json ToJson() const {
        return {
            {
                "info", {
                    {"player", this->player.ToJson()},
                    {"death_count", this->death_count},
                }
            }
        };
    }
};



void(*ApplyDmgCallbackContinue)(struct ChrDamageModule* module, Instance* entity_instance, struct DamageStruct* damage, uint64_t param4, char param5);
void ApplyDmgCallbackFunction(struct ChrDamageModule* module, Instance* sender, struct DamageStruct* damage, uint64_t param4, char param5) {
    Instance* receiver = *(Instance**)((uint8_t*)module + 0x8);
    if(receiver) {
        bool receiver_is_player = Game_CharacterIsPlayer(receiver);
        bool sender_is_player = Game_CharacterIsPlayer(sender);
        if(damage->damage > 0) {
            DamageMessage msg = {};
            msg.sender.max_hp = Game_GetCharacterMaxHp(sender);
            msg.sender.hp = Game_GetCharacterCurrentHp(sender);
            msg.sender.is_player = sender_is_player;
            msg.sender.npc_id = Game_GetNpcId(sender);
            msg.sender.pos = Game_GetCharacterPosition(sender);
            msg.sender.handle = Game_GetCharacterHandle(sender);

            Vector2 map_size = Game_GetMapSize();
            msg.sender.map_pos = Game_WorldToMap(sender);
            msg.sender.normalized_map_pos = {msg.sender.pos.x / map_size.x, msg.sender.pos.y / map_size.y};


            msg.receiver.max_hp = Game_GetCharacterMaxHp(receiver);
            msg.receiver.hp = std::max((int)Game_GetCharacterCurrentHp(receiver) - damage->damage, 0);
            msg.receiver.is_player = receiver_is_player;
            msg.receiver.npc_id = Game_GetNpcId(receiver);
            msg.receiver.pos = Game_GetCharacterPosition(receiver);
            msg.receiver.handle = Game_GetCharacterHandle(receiver);
            msg.receiver.map_pos = Game_WorldToMap(receiver);
            msg.receiver.normalized_map_pos = {msg.receiver.pos.x / map_size.x, msg.receiver.pos.y / map_size.y};
            msg.damage_value = damage->damage;

            nlohmann::json dmg_json = msg.ToJson();
            Net_SendData(dmg_json.dump());
            if(receiver_is_player && msg.receiver.hp == 0) {
                DeathMessage death_msg = {};
                death_msg.player = msg.receiver;
                death_msg.death_count = Game_GetCurrentDeathCount();
                nlohmann::json death_json = death_msg.ToJson();
                Net_SendData(death_json.dump());
            }
        }
    }
    return ApplyDmgCallbackContinue(module, sender, damage, param4, param5);
}
std::string GetLocalPlayerData() {
    Instance* local_instance = Game_GetLocalInstance();
    if(local_instance) {
        InfoMessage info = {};
        info.player.pos = Game_GetCharacterPosition(local_instance);
        info.player.max_hp = Game_GetCharacterMaxHp(local_instance);
        info.player.hp = Game_GetCharacterCurrentHp(local_instance);
        info.player.handle = Game_GetCharacterHandle(local_instance);
        info.player.map_pos = Game_WorldToMap(local_instance);
        Vector2 map_size = Game_GetMapSize();
        info.player.normalized_map_pos = {info.player.map_pos.x / map_size.x, info.player.map_pos.y / map_size.y};
        info.player.is_player = true;
        info.player.npc_id = Game_GetNpcId(local_instance);
        info.death_count = Game_GetCurrentDeathCount();
        nlohmann::json info_json = info.ToJson();
        return info_json.dump();
    }
    return "";
}

Vector2 WorldPositionToMapPosition(struct WorldMapLegacyConverter* legacy_converter, Vector3 pos, uint32_t special_map_id) {
    struct WorldMapAreaConverter* converter = (struct WorldMapAreaConverter*)((uint8_t*)legacy_converter + 0x38);
    float xmm3 = pos.x;
    float xmm1 = 0.0f;

    uint32_t eax = special_map_id;
    uint32_t ecx = eax;

    ecx = ecx >> 0x10;
    eax = eax >> 0x08;

    ecx = (uint32_t)(uint8_t)ecx;
    xmm1 = (float)(int32_t)ecx;

    ecx = (uint32_t)*((uint8_t*)converter + 0x0A);
    float xmm0 = (float)ecx;

    ecx = (uint32_t)(uint8_t)eax;
    
    eax = (uint32_t)*((uint8_t*)converter + 0x09);

    xmm1 = xmm1 - xmm0;

    xmm0 = (float)eax;

    xmm1 = xmm1 * 256.0f; // start_protected_game.exe + 0x29CE8B4
    
    xmm3 = xmm3 + xmm1;
    xmm1 = 0.0f;
    xmm1 = (float)(int32_t)ecx;

    xmm3 = xmm3 - *(float*)((uint8_t*)converter + 0x0C);
    xmm1 = xmm1 - xmm0;

    xmm0 = pos.z;

    float rsp_40 = xmm3;
    xmm1 = xmm1 * 256.0f; // start_protected_game.exe + 0x29CE8B4
    xmm0 = xmm0 + xmm1;
    xmm0 = xmm0 - *(float*)((uint8_t*)converter + 0x14);
    xmm0 = -xmm0;
    float rsp_44 = xmm0;
    float rsp_30 = rsp_40;
    float rsp_34 = rsp_44;
    xmm1 = rsp_30;
    xmm1 = xmm1 * (*(float*)((uint8_t*)converter + 0x20));
    float xmm2 = rsp_34;
    xmm2 = xmm2 * (*(float*)((uint8_t*)converter + 0x20));

    xmm1 = xmm1 + (*(float*)((uint8_t*)converter + 0x18));
    xmm2 = xmm2 + (*(float*)((uint8_t*)converter + 0x1C));


    return {xmm1, xmm2};
}

Vector2 WorldPositionToMapPosition(Vector3 pos, uint32_t map_id) {
    uint32_t v1 = (uint8_t)(map_id >> 0x10);
    uint32_t v2 = (uint8_t)(map_id >> 0x8);
    // the map id is the reason the conversion doesn't work correctly, 
    // it is good to use in some cases, but in others the bottom one is used
    // but it seems that the bottom one gets changed up a bit inbetween
    // so all in all just leave it as is and use the internal function
    WorldMapLegacyConverter* converter = Game_GetWorldMapLegacyConverter();
    if(map_id != 0xFFFFFFFF && v1 != 0 && v2 != 0) {
        return WorldPositionToMapPosition(converter, pos, map_id);
    }
    else {
        if(converter) {
            uint8_t* next = *(uint8_t**)((uint8_t*)converter + 0x10);
            if(next) {
                next = *(uint8_t**)(next + 0x8);
                if(next) {
                    uint32_t special_map_id = *(uint32_t*)(next + 0x20);
                    if(special_map_id != 0xFFFFFFFF) {
                        return WorldPositionToMapPosition(converter, pos, special_map_id);
                    }
                }
            }
        }
    }
    return {FLT_MAX, FLT_MAX};
}
Vector2 CharacterMapPosition(Instance* instance_ptr) {
    if(instance_ptr) {
        Vector3 pos = Game_GetCharacterPosition(instance_ptr);
        if(pos.x != FLT_MAX && pos.y != FLT_MAX && pos.z != FLT_MAX) {
            uint32_t map_id = Game_GetCharacterMapId(instance_ptr);
            return WorldPositionToMapPosition(pos, map_id);
        }
    }
    return {FLT_MAX, FLT_MAX};
}
