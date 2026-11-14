#include "IndividualProgression.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"

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
            { "set",  SEC_GAMEMASTER, true, &HandleSetCommand,  "Syntax: .progression set $stage [$name]\nSets the stage of the character (10, 20 ... 70)." },
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
            handler->PSendSysMessage("%s: этап не задан (будет назначен при входе).", name.c_str());
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
            handler->PSendSysMessage("Нет такого этапа: %s. Этапы: 10, 20, 30, 40, 50, 60, 70.", argStr.substr(0, space).c_str());
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
