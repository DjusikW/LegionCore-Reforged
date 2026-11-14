#ifndef MOD_INDIVIDUAL_PROGRESSION_H
#define MOD_INDIVIDUAL_PROGRESSION_H

#include "Define.h"
#include "ObjectGuid.h"
#include <map>
#include <mutex>
#include <regex>
#include <string>
#include <unordered_map>

class Player;
class Quest;
class WorldSession;

// Personal progression: every character has a stage (10 Classic, 20 Burning Crusade ... 70 Legion).
// Stages are cached for logged-in characters; the cache is shared by all map threads.
class IndividualProgression
{
public:
    static IndividualProgression* instance();

    void LoadConfig();
    void LoadData();                    // world tables: maps, zones, quests and final bosses of the stages

    bool IsEnabled() const { return _enabled; }
    bool IsKnownStage(uint8 stage) const { return _levelCaps.count(stage) != 0; }
    std::string StageName(uint8 stage) const;
    uint8 LevelCap(uint8 stage) const { auto itr = _levelCaps.find(stage); return itr != _levelCaps.end() ? itr->second : 0; }

    // Stage of a logged-in character (0 when unknown, excluded or the module is disabled: no limits).
    uint8 GetStage(ObjectGuid const& guid) const;
    // Stores the stage in the cache (when the character is online) and in the database.
    void SetStage(ObjectGuid const& guid, uint8 stage);
    // Stage of any character, online or not (database), 0 when it has none.
    uint8 QueryStage(ObjectGuid const& guid) const;

    // Stage needed for a map / zone / quest (0: open to all), stage reached by killing a creature (0: none).
    uint8 MapStage(uint32 mapId) const { return Find(_mapStages, mapId); }
    uint8 ZoneStage(uint32 zoneId) const { return Find(_zoneStages, zoneId); }
    uint8 QuestStage(Quest const* quest) const;
    uint8 BossStage(uint32 entry) const;

    // Moves a logged-in character up to the stage (never down) and tells him.
    void Advance(Player* player, uint8 stage);
    // Race/class creation rules by the highest stage of the account.
    bool CanCreate(WorldSession* session, uint8 race, uint8 playerClass) const;

    void OnLogin(Player* player);
    void OnLogout(ObjectGuid const& guid);

private:
    uint8 StageForLevel(uint8 level) const;
    bool IsExcludedAccount(uint32 accountId) const;
    static uint8 Find(std::unordered_map<uint32, uint8> const& table, uint32 key)
    {
        auto itr = table.find(key);
        return itr != table.end() ? itr->second : 0;
    }

    bool _enabled = true;
    uint8 _startingStage = 10;
    uint8 _deathKnightUnlockStage = 20;
    uint8 _deathKnightStartStage = 20;
    uint8 _demonHunterUnlockStage = 70;
    uint8 _alliedRacesUnlockStage = 70;
    std::string _excludedAccounts;
    std::regex _excludedAccountsRegex;
    std::map<uint8, uint8> _levelCaps;  // stage -> max level, ordered by stage
    std::unordered_map<uint32, uint8> _mapStages;
    std::unordered_map<uint32, uint8> _zoneStages;
    std::unordered_map<uint32, uint8> _questStages;
    std::unordered_map<uint32, uint8> _bossStages;
    std::unordered_map<uint32, uint8> _customBossStages;  // IndividualProgression.CustomProgression

    mutable std::mutex _lock;
    std::unordered_map<ObjectGuid::LowType, uint8> _stages;
};

#define sIndividualProgression IndividualProgression::instance()

#endif
