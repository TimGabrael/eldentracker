#include "functionality.h"
#include <iostream>
#include "settings.h"
#include "logger.h"
#include "networking.h"
#define NOMINMAX
#include <windows.h>

struct EntityMessage {
    void FillFromInstance(Instance* instance) {
        Vector2 map_size = Game_GetMapSize();
        this->handle = Game_GetCharacterHandle(instance);
        this->pos = Game_GetCharacterPosition(instance);
        this->map_pos = Game_WorldToMap(instance);
        this->normalized_map_pos = {this->map_pos.x / map_size.x, this->map_pos.y / map_size.y};
        this->hp = Game_GetCharacterCurrentHp(instance);
        this->max_hp = Game_GetCharacterMaxHp(instance);
        this->npc_id = Game_GetNpcId(instance);
        this->is_player = Game_CharacterIsPlayer(instance);
    }
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
struct SpawnMessage {
    EntityMessage player;
    uint32_t death_count;
    nlohmann::json ToJson() const {
        return {
            {
                "spawn", {
                    {"player", this->player.ToJson()},
                    {"death_count", this->death_count},
                }
            }
        };
    }


};
static uint32_t cur_player_anim_id = 0x0;



void(*ApplyDmgCallbackContinue)(struct ChrDamageModule* module, Instance* entity_instance, struct DamageStruct* damage, uint64_t param4, char param5);
void ApplyDmgCallbackFunction(struct ChrDamageModule* module, Instance* sender, struct DamageStruct* damage, uint64_t param4, char param5) {
    Instance* receiver = *(Instance**)((uint8_t*)module + 0x8);
    if(receiver) {
        bool receiver_is_player = Game_CharacterIsPlayer(receiver);
        bool sender_is_player = Game_CharacterIsPlayer(sender);
        if(damage->damage > 0) {
            DamageMessage msg = {};
            msg.sender.FillFromInstance(sender);
            msg.receiver.FillFromInstance(receiver);
            msg.damage_value = damage->damage;
            if(msg.receiver.hp != 0 && msg.sender.handle != 0) {
                nlohmann::json dmg_json = msg.ToJson();
                Net_SendData(dmg_json.dump());
            }
        }
    }
    return ApplyDmgCallbackContinue(module, sender, damage, param4, param5);
}
void(*SetAnimationCallbackContinue)(void* time_act_module, uint32_t anim_id);
void SetAnimationCallbackFunction(void* time_act_module, uint32_t anim_id) {
    if(time_act_module) {
        Instance* instance = *(Instance**)((uint8_t*)time_act_module + 0x8);
        if(Game_CharacterIsPlayer(instance)) {
            if(cur_player_anim_id != anim_id) {
                const uint32_t prev_anim_id = cur_player_anim_id;
                cur_player_anim_id = anim_id;
                if((anim_id == 0xF618 && prev_anim_id != 0xFD2) || (anim_id == 0xFD2 && prev_anim_id != 0xF618)) { // LOAD IN ANIMATION / ENTER GAME
                    // send spawn msg
                    SpawnMessage msg = {};
                    msg.player.FillFromInstance(instance);
                    msg.death_count = Game_GetCurrentDeathCount();
                    nlohmann::json msg_json = msg.ToJson();
                    Net_SendData(msg_json.dump());
                }
                else if(anim_id == 0x465C  || anim_id == 0x4652  ||
                        anim_id == 0x1005  || anim_id == 0x1019  ||
                        anim_id == 0x1018  || anim_id == 0x11172 ||
                        anim_id == 0x1C909 || anim_id == 0x46AA  ||
                        anim_id == 0x46DC  || anim_id == 0x467A) { // DEAD
                    DeathMessage msg = {};
                    msg.player.FillFromInstance(instance);
                    msg.death_count = Game_GetCurrentDeathCount();
                    nlohmann::json msg_json = msg.ToJson();
                    Net_SendData(msg_json.dump());
                }
                else if(anim_id == 0x109AB) { // RESTING AT GRACE
                }
            }
        }

    }
    return SetAnimationCallbackContinue(time_act_module, anim_id);
}
std::string GetLocalPlayerData() {
    Instance* local_instance = Game_GetLocalInstance();
    if(local_instance) {
        InfoMessage info = {};
        info.player.FillFromInstance(local_instance);
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
