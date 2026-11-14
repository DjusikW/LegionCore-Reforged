// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 DjusikW <https://github.com/DjusikW>
// mod-individual-progression is licensed under the GNU AGPL v3 or later, see LICENSE in this folder.

#include "IndividualProgression.h"
#include "AccountMgr.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "QuestDef.h"
#include "WorldSession.h"
#include <sstream>

IndividualProgression* IndividualProgression::instance()
{
    static IndividualProgression instance;
    return &instance;
}

static uint8 ReadStage(char const* name, uint8 def)
{
    return uint8(sConfigMgr->GetIntDefault(std::string("IndividualProgression.") + name, def));
}

void IndividualProgression::LoadConfig()
{
    _enabled = sConfigMgr->GetBoolDefault("IndividualProgression.Enable", true);

    static std::pair<uint8, uint8> const defaultCaps[] = { {0, 60}, {10, 70}, {20, 80}, {30, 85}, {40, 90}, {50, 100}, {60, 110} };
    _levelCaps.clear();
    for (auto const& cap : defaultCaps)
        _levelCaps[cap.first] = uint8(sConfigMgr->GetIntDefault("IndividualProgression.LevelCap." + std::to_string(cap.first), cap.second));

    _startingStage = ReadStage("StartingStage", 0);
    if (!LevelCap(_startingStage))
    {
        TC_LOG_ERROR("server.loading", "[mod-individual-progression] StartingStage %u is not a stage, using 0", _startingStage);
        _startingStage = 0;
    }

    _deathKnightUnlockStage = ReadStage("DeathKnightUnlockStage", 10);
    _deathKnightStartStage = ReadStage("DeathKnightStartStage", 10);
    _demonHunterUnlockStage = ReadStage("DemonHunterUnlockStage", 60);
    _alliedRacesUnlockStage = ReadStage("AlliedRacesUnlockStage", 60);

    // "entry stage, entry stage": killing the creature moves the group to the stage
    _customBossStages.clear();
    std::string custom = sConfigMgr->GetStringDefault("IndividualProgression.CustomProgression", "");
    std::istringstream pairs(custom);
    for (std::string pair; std::getline(pairs, pair, ',');)
    {
        std::istringstream fields(pair);
        uint32 entry = 0, stage = 0;
        if (fields >> entry >> stage && stage <= 255 && LevelCap(uint8(stage)))
            _customBossStages[entry] = uint8(stage);
        else if (pair.find_first_not_of(' ') != std::string::npos)
            TC_LOG_ERROR("server.loading", "[mod-individual-progression] CustomProgression: bad pair '%s'", pair.c_str());
    }

    _excludedAccounts = sConfigMgr->GetStringDefault("IndividualProgression.ExcludedAccountsRegex", "^PLAYERBOT");
    try
    {
        _excludedAccountsRegex = std::regex(_excludedAccounts, std::regex::icase);
    }
    catch (std::regex_error const&)
    {
        TC_LOG_ERROR("server.loading", "[mod-individual-progression] ExcludedAccountsRegex '%s' is not a regex, no account excluded", _excludedAccounts.c_str());
        _excludedAccounts.clear();
    }
}

static void LoadTable(char const* query, std::unordered_map<uint32, uint8>& table)
{
    table.clear();
    if (QueryResult result = WorldDatabase.Query(query))
        do
            table[(*result)[0].GetUInt32()] = (*result)[1].GetUInt8();
        while (result->NextRow());
}

void IndividualProgression::LoadData()
{
    _stageNames.clear();
    if (QueryResult result = WorldDatabase.Query("SELECT stage, name FROM individual_progression_stage"))
        do
            _stageNames[(*result)[0].GetUInt8()] = (*result)[1].GetString();
        while (result->NextRow());
    if (_stageNames.empty())    // no data yet: one stage per expansion
        for (auto const& cap : _levelCaps)
            _stageNames[cap.first] = "";

    LoadTable("SELECT map_id, stage FROM individual_progression_map", _mapStages);
    LoadTable("SELECT zone_id, stage FROM individual_progression_zone", _zoneStages);
    LoadTable("SELECT quest_id, stage FROM individual_progression_quest", _questStages);
    LoadTable("SELECT creature_entry, stage FROM individual_progression_boss", _bossStages);

    _encounters.clear();
    if (QueryResult result = WorldDatabase.Query("SELECT map_id, boss_id, stage FROM individual_progression_encounter"))
        do
            _encounters.push_back({ (*result)[0].GetUInt32(), (*result)[1].GetUInt32(), (*result)[2].GetUInt8() });
        while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> [mod-individual-progression] %u stages, %u maps, %u zones, %u quests, %u final bosses, %u encounters",
        uint32(_stageNames.size()), uint32(_mapStages.size()), uint32(_zoneStages.size()), uint32(_questStages.size()), uint32(_bossStages.size()), uint32(_encounters.size()));
}

std::string IndividualProgression::StageName(uint8 stage) const
{
    auto itr = _stageNames.find(stage);
    if (itr != _stageNames.end() && !itr->second.empty())
        return itr->second;

    switch (stage / 10)
    {
        case 1: return "Классика";
        case 2: return "Burning Crusade";
        case 3: return "Wrath of the Lich King";
        case 4: return "Катаклизм";
        case 5: return "Пандария";
        case 6: return "Дренор";
        case 7: return "Легион";
        default: return "?";
    }
}

std::string IndividualProgression::StageList() const
{
    std::string list;
    for (auto const& stage : _stageNames)
        list += (list.empty() ? "" : ", ") + std::to_string(stage.first);
    return list;
}

uint8 IndividualProgression::GetStage(ObjectGuid const& guid) const
{
    std::lock_guard<std::mutex> guard(_lock);
    auto itr = _stages.find(guid.GetCounter());
    return itr != _stages.end() ? itr->second : NO_STAGE;
}

void IndividualProgression::SetStage(ObjectGuid const& guid, uint8 stage)
{
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto itr = _stages.find(guid.GetCounter());
        if (itr != _stages.end())
            itr->second = stage;
    }
    CharacterDatabase.PExecute("REPLACE INTO character_progression (guid, stage) VALUES (" UI64FMTD ", %u)", guid.GetCounter(), stage);
}

uint8 IndividualProgression::QueryStage(ObjectGuid const& guid) const
{
    if (QueryResult result = CharacterDatabase.PQuery("SELECT stage FROM character_progression WHERE guid = " UI64FMTD, guid.GetCounter()))
        return (*result)[0].GetUInt8();
    return NO_STAGE;
}

uint8 IndividualProgression::QuestStage(Quest const* quest) const
{
    if (uint8 stage = Find(_questStages, quest->GetQuestId()))
        return stage;

    // Quests of a later expansion above the first cap (level 61+): the expansion's own zones and breadcrumbs.
    // Lower ones (Pandaren and death knight starting quests) stay open.
    if (quest->Expansion >= 1 && quest->Expansion <= 6 && quest->MinLevel > _levelCaps.begin()->second)
        return uint8(quest->Expansion * 10);
    return 0;
}

uint8 IndividualProgression::BossStage(uint32 entry) const
{
    if (uint8 stage = Find(_customBossStages, entry))
        return stage;
    return Find(_bossStages, entry);
}

uint8 IndividualProgression::StageForLevel(uint8 level) const
{
    for (auto const& cap : _levelCaps)
        if (level <= cap.second)
            return cap.first;
    return _levelCaps.rbegin()->first;
}

bool IndividualProgression::IsExcludedAccount(uint32 accountId) const
{
    if (_excludedAccounts.empty())
        return false;

    std::string name;
    return AccountMgr::GetName(accountId, name) && std::regex_search(name, _excludedAccountsRegex);
}

void IndividualProgression::Advance(Player* player, uint8 stage)
{
    uint8 current = GetStage(player->GetGUID());
    if (current == NO_STAGE || current >= stage)
        return;

    SetStage(player->GetGUID(), stage);
    ChatHandler(player).PSendSysMessage("|cff00ff00Этап пройден!|r Новый этап: %u (%s), предел уровня %u.", stage, StageName(stage).c_str(), LevelCap(stage));
}

void IndividualProgression::CheckLevelCap(Player* player)
{
    uint8 stage = GetStage(player->GetGUID());
    if (stage != NO_STAGE && stage % 10 == 0 && player->getLevel() >= LevelCap(stage) && IsKnownStage(stage + 1))
        Advance(player, stage + 1);
}

bool IndividualProgression::CanCreate(WorldSession* session, uint8 race, uint8 playerClass) const
{
    uint8 need = 0;
    if (playerClass == CLASS_DEATH_KNIGHT)
        need = _deathKnightUnlockStage;
    else if (playerClass == CLASS_DEMON_HUNTER)
        need = _demonHunterUnlockStage;
    if (race == RACE_NIGHTBORNE || race == RACE_HIGHMOUNTAIN_TAUREN || race == RACE_VOID_ELF || race == RACE_LIGHTFORGED_DRAENEI)
        need = std::max(need, _alliedRacesUnlockStage);

    if (!need || IsExcludedAccount(session->GetAccountId()))
        return true;

    uint8 best = 0;
    if (QueryResult result = CharacterDatabase.PQuery("SELECT MAX(p.stage) FROM character_progression p JOIN characters c ON c.guid = p.guid WHERE c.account = %u", session->GetAccountId()))
        best = (*result)[0].GetUInt8();
    return best >= need;
}

void IndividualProgression::OnLogin(Player* player)
{
    if (IsExcludedAccount(player->GetSession()->GetAccountId()))
        return;     // no stage: no limits

    ObjectGuid guid = player->GetGUID();
    uint8 stage = QueryStage(guid);
    if (stage == NO_STAGE)
    {
        // First login with the module: new characters start at StartingStage, older ones get the stage of their level.
        stage = std::max(_startingStage, StageForLevel(player->getLevel()));
        if (player->getClass() == CLASS_DEATH_KNIGHT)
            stage = std::max(stage, _deathKnightStartStage);
        CharacterDatabase.PExecute("REPLACE INTO character_progression (guid, stage) VALUES (" UI64FMTD ", %u)", guid.GetCounter(), stage);
    }

    {
        std::lock_guard<std::mutex> guard(_lock);
        _stages[guid.GetCounter()] = stage;
    }

    ChatHandler(player).PSendSysMessage("Этап прогрессии: %u (%s), предел уровня %u.", stage, StageName(stage).c_str(), LevelCap(stage));
    CheckLevelCap(player);
}

void IndividualProgression::OnLogout(ObjectGuid const& guid)
{
    std::lock_guard<std::mutex> guard(_lock);
    _stages.erase(guid.GetCounter());
}
