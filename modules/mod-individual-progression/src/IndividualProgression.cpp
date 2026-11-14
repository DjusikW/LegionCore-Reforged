#include "IndividualProgression.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"

IndividualProgression* IndividualProgression::instance()
{
    static IndividualProgression instance;
    return &instance;
}

void IndividualProgression::LoadConfig()
{
    _enabled = sConfigMgr->GetBoolDefault("IndividualProgression.Enable", true);

    static std::pair<uint8, uint8> const defaultCaps[] = { {10, 60}, {20, 70}, {30, 80}, {40, 85}, {50, 90}, {60, 100}, {70, 110} };
    _levelCaps.clear();
    for (auto const& cap : defaultCaps)
        _levelCaps[cap.first] = uint8(sConfigMgr->GetIntDefault("IndividualProgression.LevelCap." + std::to_string(cap.first), cap.second));

    _startingStage = uint8(sConfigMgr->GetIntDefault("IndividualProgression.StartingStage", 10));
    if (!IsKnownStage(_startingStage))
    {
        TC_LOG_ERROR("server.loading", "[mod-individual-progression] StartingStage %u is not a stage, using 10", _startingStage);
        _startingStage = 10;
    }
}

void IndividualProgression::LoadData()
{
    _mapStages.clear();
    if (QueryResult result = WorldDatabase.Query("SELECT map_id, stage FROM individual_progression_map"))
        do
            _mapStages[(*result)[0].GetUInt32()] = (*result)[1].GetUInt8();
        while (result->NextRow());

    _bossStages.clear();
    if (QueryResult result = WorldDatabase.Query("SELECT creature_entry, stage FROM individual_progression_boss"))
        do
            _bossStages[(*result)[0].GetUInt32()] = (*result)[1].GetUInt8();
        while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> [mod-individual-progression] %u dungeon/raid maps, %u final bosses",
        uint32(_mapStages.size()), uint32(_bossStages.size()));
}

std::string IndividualProgression::StageName(uint8 stage) const
{
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

uint8 IndividualProgression::GetStage(ObjectGuid const& guid) const
{
    std::lock_guard<std::mutex> guard(_lock);
    auto itr = _stages.find(guid.GetCounter());
    return itr != _stages.end() ? itr->second : 0;
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
    return 0;
}

uint8 IndividualProgression::StageForLevel(uint8 level) const
{
    for (auto const& cap : _levelCaps)
        if (level <= cap.second)
            return cap.first;
    return _levelCaps.rbegin()->first;
}

void IndividualProgression::OnLogin(Player* player)
{
    ObjectGuid guid = player->GetGUID();
    uint8 stage = QueryStage(guid);
    if (!stage)
    {
        // First login with the module: new characters start at StartingStage, older ones get the stage of their level.
        stage = std::max(_startingStage, StageForLevel(player->getLevel()));
        CharacterDatabase.PExecute("REPLACE INTO character_progression (guid, stage) VALUES (" UI64FMTD ", %u)", guid.GetCounter(), stage);
    }

    {
        std::lock_guard<std::mutex> guard(_lock);
        _stages[guid.GetCounter()] = stage;
    }

    ChatHandler(player).PSendSysMessage("Этап прогрессии: %u (%s), предел уровня %u.", stage, StageName(stage).c_str(), _levelCaps.count(stage) ? _levelCaps.at(stage) : 0);
}

void IndividualProgression::OnLogout(ObjectGuid const& guid)
{
    std::lock_guard<std::mutex> guard(_lock);
    _stages.erase(guid.GetCounter());
}
