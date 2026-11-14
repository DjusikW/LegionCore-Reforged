// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 DjusikW <https://github.com/DjusikW>
// mod-individual-progression is licensed under the GNU AGPL v3 or later, see LICENSE in this folder.

#include "IndividualProgression.h"
#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
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
        if (!stage)     // no stage is reached by a kill: 0 means "not a final boss"
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

    // Encounters without a creature death (Madness of Deathwing): everyone in the instance moves on.
    void OnInstanceBossDone(InstanceMap* map, uint32 bossId) override
    {
        if (!sIndividualProgression->IsEnabled())
            return;

        for (auto const& encounter : sIndividualProgression->Encounters())
        {
            if (encounter.mapId != map->GetId() || encounter.bossId != bossId)
                continue;

            TC_LOG_INFO("server.scripts", "[mod-individual-progression] boss %u done on map %u: stage %u", bossId, map->GetId(), encounter.stage);
            for (auto const& ref : map->GetPlayers())
                if (Player* player = ref.getSource())
                    sIndividualProgression->Advance(player, encounter.stage);
        }
    }

    // Dungeons, raids and continents of a later stage (also the dungeon finder, with notify = false).
    bool CanEnterMap(Player* player, MapEntry const* map, bool notify) override
    {
        if (!sIndividualProgression->IsEnabled() || !map)
            return true;

        uint8 stage = sIndividualProgression->GetStage(player->GetGUID());
        uint8 need = sIndividualProgression->MapStage(map->ID);
        if (stage == IndividualProgression::NO_STAGE || need <= stage)
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
        if (stage == IndividualProgression::NO_STAGE || need <= stage)
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
        if (stage == IndividualProgression::NO_STAGE || need <= stage)
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

    void OnLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (sIndividualProgression->IsEnabled())
            sIndividualProgression->CheckLevelCap(player);
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

// Thrall after Madness of Deathwing (Dragon Soul): closes the Cataclysm and gives the stage to whoever
// missed the moment (joined the instance after the fight).
class npc_individual_progression_thrall_ds : public CreatureScript
{
public:
    npc_individual_progression_thrall_ds() : CreatureScript("npc_individual_progression_thrall_ds") { }

    // Stage of the first encounter of this map already DONE (0: none).
    static uint8 DoneStage(Creature* creature)
    {
        InstanceScript* instance = creature->GetInstanceScript();
        if (!instance)
            return 0;

        for (auto const& encounter : sIndividualProgression->Encounters())
            if (encounter.mapId == creature->GetMapId() && instance->GetBossState(encounter.bossId) == DONE)
                return encounter.stage;
        return 0;
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (creature->isQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        if (sIndividualProgression->IsEnabled() && DoneStage(creature))
            player->ADD_GOSSIP_ITEM(GossipOptionNpc::None, "Смертокрыл повержен. Что ждет нас дальше?", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);

        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        player->CLOSE_GOSSIP_MENU();
        if (action != GOSSIP_ACTION_INFO_DEF + 1)
            return true;

        uint8 stage = DoneStage(creature);
        if (!stage)
            return true;

        ChatHandler(player).PSendSysMessage("|cffffd200Тралл:|r Эпоха Катаклизма позади, Смертокрыл больше не грозит Азероту. "
            "Отдохни, герой: за туманами на юге ждет Пандария.");
        sIndividualProgression->Advance(player, stage);
        return true;
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
            { "set",  SEC_GAMEMASTER, true, &HandleSetCommand,  "Syntax: .progression set $stage [$name]\nSets the stage of the character (0-3 Classic, 10-14 Burning Crusade ... 60-65 Legion)." },
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
        uint8 stage = sIndividualProgression->GetStage(guid);
        return stage != IndividualProgression::NO_STAGE ? stage : sIndividualProgression->QueryStage(guid);
    }

    static bool HandleShowCommand(ChatHandler* handler, char const* args)
    {
        Player* target = nullptr;
        ObjectGuid guid;
        std::string name;
        if (!handler->extractPlayerTarget(const_cast<char*>(args), &target, &guid, &name))
            return false;

        uint8 stage = StageOf(guid);
        if (stage == IndividualProgression::NO_STAGE)
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
        std::string stageArg = argStr.substr(0, space);
        int32 stage = !stageArg.empty() && stageArg.find_first_not_of("0123456789") == std::string::npos ? atoi(stageArg.c_str()) : -1;
        std::string rest = space == std::string::npos ? "" : argStr.substr(space + 1);

        if (stage < 0 || stage >= IndividualProgression::NO_STAGE || !sIndividualProgression->IsKnownStage(uint8(stage)))
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
    new npc_individual_progression_thrall_ds();
    new individual_progression_commandscript();
}
