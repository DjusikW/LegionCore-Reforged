#ifndef MOD_INDIVIDUAL_PROGRESSION_H
#define MOD_INDIVIDUAL_PROGRESSION_H

#include "Define.h"
#include "ObjectGuid.h"
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>

class Player;

// Personal progression: every character has a stage (10 Classic, 20 Burning Crusade ... 70 Legion).
// Stages are cached for logged-in characters; the cache is shared by all map threads.
class IndividualProgression
{
public:
    static IndividualProgression* instance();

    void LoadConfig();
    void LoadData();                    // world tables: map -> stage, final boss -> next stage

    bool IsEnabled() const { return _enabled; }
    bool IsKnownStage(uint8 stage) const { return _levelCaps.count(stage) != 0; }
    std::string StageName(uint8 stage) const;

    // Stage of a logged-in character (0 when unknown: not loaded or module disabled).
    uint8 GetStage(ObjectGuid const& guid) const;
    // Stores the stage in the cache (when the character is online) and in the database.
    void SetStage(ObjectGuid const& guid, uint8 stage);
    // Stage of any character, online or not (database), 0 when it has none.
    uint8 QueryStage(ObjectGuid const& guid) const;

    void OnLogin(Player* player);
    void OnLogout(ObjectGuid const& guid);

private:
    uint8 StageForLevel(uint8 level) const;

    bool _enabled = true;
    uint8 _startingStage = 10;
    std::map<uint8, uint8> _levelCaps;  // stage -> max level, ordered by stage
    std::unordered_map<uint32, uint8> _mapStages;
    std::unordered_map<uint32, uint8> _bossStages;

    mutable std::mutex _lock;
    std::unordered_map<ObjectGuid::LowType, uint8> _stages;
};

#define sIndividualProgression IndividualProgression::instance()

#endif
