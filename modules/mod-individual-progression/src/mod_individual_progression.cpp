#include "IndividualProgression.h"
#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

class individual_progression_world : public WorldScript
{
public:
    individual_progression_world() : WorldScript("individual_progression_world") { }

    void OnConfigLoad(bool /*reload*/) override
    {
        sIndividualProgression->LoadConfig();
    }

    void OnStartup() override
    {
        CharacterDatabase.DirectExecute(
            "CREATE TABLE IF NOT EXISTS character_progression ("
            "guid BIGINT UNSIGNED NOT NULL, stage TINYINT UNSIGNED NOT NULL, PRIMARY KEY (guid)) "
            "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
        sIndividualProgression->LoadData();
    }
};

class individual_progression_player : public PlayerScript
{
public:
    individual_progression_player() : PlayerScript("individual_progression_player") { }

    void OnLogin(Player* player) override
    {
        if (sIndividualProgression->IsEnabled())
            sIndividualProgression->OnLogin(player);
    }

    // No experience at the level cap of the stage (GM .levelup is not limited).
    void OnGiveXP(Player* player, uint32& amount, Unit* /*victim*/) override
    {
        if (!sIndividualProgression->IsEnabled())
            return;

        uint8 cap = sIndividualProgression->LevelCap(sIndividualProgression->GetStage(player->GetGUID()));
        if (cap && player->getLevel() >= cap)
            amount = 0;
    }

    // Final boss of a stage: everyone in the instance (or the killer's group nearby, outside instances) moves on.
    void OnCreatureDeath(Creature* killed, Unit* killer) override
    {
        if (!sIndividualProgression->IsEnabled())
            return;

        uint8 stage = sIndividualProgression->BossStage(killed->GetEntry());
        if (!stage)
            return;

        Map* map = killed->GetMap();
        TC_LOG_INFO("server.scripts", "[mod-individual-progression] %u (%s) killed on map %u: stage %u", killed->GetEntry(), killed->GetName(), map->GetId(), stage);

        if (map->IsDungeon())
        {
            for (auto const& ref : map->GetPlayers())
                if (Player* player = ref.getSource())
                    sIndividualProgression->Advance(player, stage);
            return;
        }

        Player* player = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        if (!player)
            return;
        if (Group* group = player->GetGroup())
            group->ForEachMemberInMap(map, [stage](Player* member) { sIndividualProgression->Advance(member, stage); });
        else
            sIndividualProgression->Advance(player, stage);
    }

    // Dungeons, raids and continents of a later stage (also the dungeon finder, with notify = false).
    bool CanEnterMap(Player* player, MapEntry const* map, bool notify) override
    {
        if (!sIndividualProgression->IsEnabled() || !map)
            return true;

        uint8 stage = sIndividualProgression->GetStage(player->GetGUID());
        uint8 need = sIndividualProgression->MapStage(map->ID);
        if (!stage || need <= stage)
            return true;

        if (notify)
            ChatHandler(player).PSendSysMessage("|cffff4040Недоступно:|r %s открывается на этапе %u (%s). Ваш этап: %u.",
                map->MapName->Str[player->GetSession()->GetSessionDbcLocale()], need, sIndividualProgression->StageName(need).c_str(), stage);
        return false;
    }

    // Zones of a later stage on a shared map (Outland on the map of Eversong, Cataclysm zones): back to the hearthstone.
    void OnUpdateZone(Player* player, uint32 newZone, uint32 /*newArea*/) override
    {
        if (!sIndividualProgression->IsEnabled() || player->isGameMaster())
            return;

        uint8 stage = sIndividualProgression->GetStage(player->GetGUID());
        uint8 need = sIndividualProgression->ZoneStage(newZone);
        if (!stage || need <= stage)
            return;

        ChatHandler(player).PSendSysMessage("|cffff4040Недоступно:|r эта зона открывается на этапе %u (%s). Ваш этап: %u. Возврат к камню возвращения.",
            need, sIndividualProgression->StageName(need).c_str(), stage);
        player->TeleportTo(player->m_homebindMapId, player->m_homebindX, player->m_homebindY, player->m_homebindZ, player->GetOrientation());
    }

    // Introductory quests and quests of later expansions.
    bool CanTakeQuest(Player* player, Quest const* quest, bool msg) override
    {
        if (!sIndividualProgression->IsEnabled())
            return true;

        uint8 stage = sIndividualProgression->GetStage(player->GetGUID());
        uint8 need = sIndividualProgression->QuestStage(quest);
        if (!stage || need <= stage)
            return true;

        if (msg)
            ChatHandler(player).PSendSysMessage("|cffff4040Недоступно:|r задание открывается на этапе %u (%s). Ваш этап: %u.",
                need, sIndividualProgression->StageName(need).c_str(), stage);
        return false;
    }

    // Death knights, demon hunters and allied races need a character of the unlock stage on the account.
    bool CanCreateCharacter(WorldSession* session, uint8 race, uint8 playerClass) override
    {
        return !sIndividualProgression->IsEnabled() || sIndividualProgression->CanCreate(session, race, playerClass);
    }

    void OnLogout(Player* player) override
    {
        sIndividualProgression->OnLogout(player->GetGUID());
    }

    void OnDelete(ObjectGuid const& guid) override
    {
        CharacterDatabase.PExecute("DELETE FROM character_progression WHERE guid = " UI64FMTD, guid.GetCounter());
    }
};

class individual_progression_commandscript : public CommandScript
{
public:
    individual_progression_commandscript() : CommandScript("individual_progression_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> progressionCommandTable =
        {
            { "show", SEC_GAMEMASTER, true, &HandleShowCommand, "Syntax: .progression show [$name]\nStage of the character." },
            { "set",  SEC_GAMEMASTER, true, &HandleSetCommand,  "Syntax: .progression set $stage [$name]\nSets the stage of the character (10, 11, 12, 20 ... 74)." },
        };
        static std::vector<ChatCommand> commandTable =
        {
            { "progression", SEC_GAMEMASTER, true, nullptr, "", progressionCommandTable },
        };
        return commandTable;
    }

    // Stage of an online character comes from the cache, of an offline one from the database.
    static uint8 StageOf(ObjectGuid const& guid)
    {
        if (uint8 stage = sIndividualProgression->GetStage(guid))
            return stage;
        return sIndividualProgression->QueryStage(guid);
    }

    static bool HandleShowCommand(ChatHandler* handler, char const* args)
    {
        Player* target = nullptr;
        ObjectGuid guid;
        std::string name;
        if (!handler->extractPlayerTarget(const_cast<char*>(args), &target, &guid, &name))
            return false;

        uint8 stage = StageOf(guid);
        if (!stage)
            handler->PSendSysMessage("%s: этап не задан (будет назначен при входе; боты без этапа).", name.c_str());
        else
            handler->PSendSysMessage("%s: этап %u (%s).", name.c_str(), stage, sIndividualProgression->StageName(stage).c_str());
        return true;
    }

    static bool HandleSetCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
            return false;

        std::string argStr(args);
        size_t space = argStr.find(' ');
        int32 stage = atoi(argStr.substr(0, space).c_str());
        std::string rest = space == std::string::npos ? "" : argStr.substr(space + 1);

        if (stage <= 0 || stage > 255 || !sIndividualProgression->IsKnownStage(uint8(stage)))
        {
            handler->PSendSysMessage("Нет такого этапа: %s. Этапы: %s.", argStr.substr(0, space).c_str(), sIndividualProgression->StageList().c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* target = nullptr;
        ObjectGuid guid;
        std::string name;
        if (!handler->extractPlayerTarget(rest.empty() ? nullptr : &rest[0], &target, &guid, &name))
            return false;

        sIndividualProgression->SetStage(guid, uint8(stage));
        handler->PSendSysMessage("%s: этап %u (%s).", name.c_str(), stage, sIndividualProgression->StageName(uint8(stage)).c_str());
        if (target && (!handler->GetSession() || target != handler->GetSession()->GetPlayer()))
            ChatHandler(target).PSendSysMessage("Ваш этап прогрессии: %u (%s).", stage, sIndividualProgression->StageName(uint8(stage)).c_str());
        return true;
    }
};

void Addmod_individual_progressionScripts()
{
    std::string error;
    if (!sConfigMgr->LoadModuleConfig("individual-progression.conf", error))
        TC_LOG_ERROR("server.loading", "[mod-individual-progression] individual-progression.conf not loaded, using defaults: %s", error.c_str());

    new individual_progression_world();
    new individual_progression_player();
    new individual_progression_commandscript();
}
