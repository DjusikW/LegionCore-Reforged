// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 DjusikW <https://github.com/DjusikW>
// mod-individual-progression is licensed under the GNU AGPL v3 or later, see LICENSE in this folder.

#ifndef MOD_INDIVIDUAL_PROGRESSION_H
#define MOD_INDIVIDUAL_PROGRESSION_H

#include "Define.h"
#include "ObjectGuid.h"
#include <map>
#include <mutex>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

class Player;
class Quest;
class WorldSession;

// Personal progression: every character has a stage (10 Classic, 20 Burning Crusade ... 70 Legion).
// Stages are cached for logged-in characters; the cache is shared by all map threads.
class IndividualProgression
{
public:
    static constexpr uint8 NO_STAGE = 255;   // not loaded, excluded account or module disabled: no limits

    static IndividualProgression* instance();

    void LoadConfig();
    void LoadData();                    // world tables: maps, zones, quests and final bosses of the stages

    bool IsEnabled() const { return _enabled; }
    // Stage x0 of an expansion is levelling and dungeons, x1... its raid tiers: 0-3 Classic, 10-14 Burning Crusade ...
    // (individual_progression_stage).
    bool IsKnownStage(uint8 stage) const { return _stageNames.count(stage) != 0; }
    std::string StageName(uint8 stage) const;
    std::string StageList() const;
    // Level cap of the stage's expansion (LevelCap.0 for stages 0-3, LevelCap.10 for 10-14 ...).
    uint8 LevelCap(uint8 stage) const { auto itr = _levelCaps.find(stage / 10 * 10); return itr != _levelCaps.end() ? itr->second : 0; }

    // Stage of a logged-in character, NO_STAGE when unknown.
    uint8 GetStage(ObjectGuid const& guid) const;
    // Stores the stage in the cache (when the character is online) and in the database.
    void SetStage(ObjectGuid const& guid, uint8 stage);
    // Stage of any character, online or not (database), NO_STAGE when it has none.
    uint8 QueryStage(ObjectGuid const& guid) const;

    // Stage needed for a map / zone / quest (0: open to all), stage reached by killing a creature (0: none).
    uint8 MapStage(uint32 mapId) const { return Find(_mapStages, mapId); }
    uint8 ZoneStage(uint32 zoneId) const { return Find(_zoneStages, zoneId); }
    uint8 QuestStage(Quest const* quest) const;
    uint8 BossStage(uint32 entry) const;

    // Encounters that end without a creature death: instance boss id set to DONE -> next stage.
    struct Encounter { uint32 mapId; uint32 bossId; uint8 stage; };
    std::vector<Encounter> const& Encounters() const { return _encounters; }

    // Moves a logged-in character up to the stage (never down) and tells him.
    void Advance(Player* player, uint8 stage);
    // Levelling stage (x0) at the level cap of its expansion: on to the first raid tier (x1).
    void CheckLevelCap(Player* player);
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
    uint8 _startingStage = 0;
    uint8 _deathKnightUnlockStage = 10;
    uint8 _deathKnightStartStage = 10;
    uint8 _demonHunterUnlockStage = 60;
    uint8 _alliedRacesUnlockStage = 60;
    std::string _excludedAccounts;
    std::regex _excludedAccountsRegex;
    std::map<uint8, uint8> _levelCaps;  // levelling stage (0, 10 ... 60) -> max level, ordered
    std::map<uint8, std::string> _stageNames;
    std::unordered_map<uint32, uint8> _mapStages;
    std::unordered_map<uint32, uint8> _zoneStages;
    std::unordered_map<uint32, uint8> _questStages;
    std::unordered_map<uint32, uint8> _bossStages;
    std::vector<Encounter> _encounters;
    std::unordered_map<uint32, uint8> _customBossStages;  // IndividualProgression.CustomProgression

    mutable std::mutex _lock;
    std::unordered_map<ObjectGuid::LowType, uint8> _stages;
};

#define sIndividualProgression IndividualProgression::instance()

#endif
