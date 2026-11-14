// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 DjusikW <https://github.com/DjusikW>
// mod-autobalance is licensed under the GNU AGPL v3 or later, see LICENSE in this folder.
// The scaling curve follows AzerothCore mod-autobalance (without level scaling).

#include "Config.h"
#include "Creature.h"
#include "Log.h"
#include "Map.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include <algorithm>
#include <cmath>

namespace
{
    // visible aura "Автобаланс" (hotfix spell, sql/hotfixes), its stack count is the number of players
    uint32 const SPELL_AUTOBALANCE_AURA = 900100;

    enum Category
    {
        CATEGORY_DUNGEON,
        CATEGORY_DUNGEON_HEROIC,
        CATEGORY_RAID,
        CATEGORY_RAID_HEROIC,
        CATEGORY_COUNT
    };

    char const* const CategoryNames[CATEGORY_COUNT] = { "Dungeon", "DungeonHeroic", "Raid", "RaidHeroic" };

    struct CategoryConfig
    {
        bool Enable = true;
        float Inflection = 0.5f;
        float BossInflection = 1.0f;
        CreatureStatModifiers Mods;
        CreatureStatModifiers BossMods;
    };

    struct AutoBalanceConfig
    {
        bool Enable = false;
        bool ShowAura = true;
        uint32 MinPlayers = 1;
        CategoryConfig Categories[CATEGORY_COUNT];
        CreatureStatModifiers Min;  // lowest allowed multipliers
    };

    AutoBalanceConfig Settings;

    CreatureStatModifiers LoadModifiers(std::string const& prefix, CreatureStatModifiers const& defaults)
    {
        CreatureStatModifiers mods;
        mods.Health = sConfigMgr->GetFloatDefault(prefix + "Health", defaults.Health);
        mods.Damage = sConfigMgr->GetFloatDefault(prefix + "Damage", defaults.Damage);
        mods.Armor = sConfigMgr->GetFloatDefault(prefix + "Armor", defaults.Armor);
        mods.Mana = sConfigMgr->GetFloatDefault(prefix + "Mana", defaults.Mana);
        return mods;
    }

    void LoadConfig()
    {
        AutoBalanceConfig config;
        config.Enable = sConfigMgr->GetBoolDefault("AutoBalance.Enable", false);
        config.ShowAura = sConfigMgr->GetBoolDefault("AutoBalance.ShowAura", true);
        config.MinPlayers = uint32(std::max(1, sConfigMgr->GetIntDefault("AutoBalance.MinPlayers", 1)));

        for (uint32 i = 0; i < CATEGORY_COUNT; ++i)
        {
            // a missing *Heroic setting takes the value of its non-heroic one
            bool heroic = i == CATEGORY_DUNGEON_HEROIC || i == CATEGORY_RAID_HEROIC;
            CategoryConfig defaults = heroic ? config.Categories[i - 1] : CategoryConfig();
            std::string name = CategoryNames[i];

            CategoryConfig& category = config.Categories[i];
            category.Enable = sConfigMgr->GetBoolDefault("AutoBalance.Enable." + name, defaults.Enable);
            category.Inflection = sConfigMgr->GetFloatDefault("AutoBalance.InflectionPoint." + name, defaults.Inflection);
            category.BossInflection = sConfigMgr->GetFloatDefault("AutoBalance.InflectionPoint." + name + ".BossModifier", defaults.BossInflection);
            category.Mods = LoadModifiers("AutoBalance.StatModifier." + name + ".", defaults.Mods);
            category.BossMods = LoadModifiers("AutoBalance.StatModifier." + name + ".Boss.", defaults.BossMods);
        }

        config.Min.Health = sConfigMgr->GetFloatDefault("AutoBalance.MinHealthModifier", 0.01f);
        config.Min.Damage = sConfigMgr->GetFloatDefault("AutoBalance.MinDamageModifier", 0.01f);
        config.Min.Armor = sConfigMgr->GetFloatDefault("AutoBalance.MinArmorModifier", 0.01f);
        config.Min.Mana = sConfigMgr->GetFloatDefault("AutoBalance.MinManaModifier", 0.01f);

        Settings = config;
        if (Settings.Enable)
            TC_LOG_INFO("server.loading", "AutoBalance enabled, minimum %u player(s) (djw-ab rev. 2611)", Settings.MinPlayers);
    }

    // Weakens creatures of dungeons and raids when there are fewer players than the difficulty is tuned for.
    // Flexible raids are tuned for their minimum size (above it the core already adds health per player),
    // the other difficulties for their maximum size. auraStacks: players shown by the aura, 0 - no aura.
    CreatureStatModifiers Calculate(Creature* creature, uint8& auraStacks)
    {
        CreatureStatModifiers mods;
        auraStacks = 0;

        Map* map = creature->GetMap();
        if (!Settings.Enable || !map->Instanceable() || !map->IsDungeon() || map->IsScenario())
            return mods;

        bool raid = map->IsRaid();
        bool heroic = map->IsHeroic();
        switch (map->GetDifficultyID())
        {
            case DIFFICULTY_HEROIC:
            case DIFFICULTY_MYTHIC_DUNGEON:
            case DIFFICULTY_MYTHIC_KEYSTONE:
            case DIFFICULTY_HEROIC_RAID:
            case DIFFICULTY_MYTHIC_RAID:
            case DIFFICULTY_10_HC:
            case DIFFICULTY_25_HC:
                heroic = true;
                break;
            default:
                break;
        }

        CategoryConfig const& category = Settings.Categories[raid ? (heroic ? CATEGORY_RAID_HEROIC : CATEGORY_RAID)
            : (heroic ? CATEGORY_DUNGEON_HEROIC : CATEGORY_DUNGEON)];
        if (!category.Enable)
            return mods;

        // pets, guardians and totems of the players keep their own stats
        if (creature->isPet() || creature->isTotem() || creature->GetCharmerOrOwnerPlayerOrPlayerItself())
            return mods;

        uint32 tuned = (raid && map->IsNeedRecalc()) ? map->GetMinPlayer() : map->GetMaxPlayer();
        if (!tuned)
            return mods;

        bool boss = creature->IsDungeonBoss() || creature->GetCreatureTemplate()->Classification == CREATURE_CLASSIFICATION_WORLDBOSS;

        float curve = 1.0f;
        uint32 count = std::max(map->GetPlayersCountExceptGMs(), Settings.MinPlayers);
        if (count < tuned)
        {
            float inflection = category.Inflection;
            if (boss)
                inflection *= category.BossInflection;

            float inflectionValue = float(tuned) * inflection;
            float diff = float(tuned) / 5.0f * 1.5f;
            curve = (std::tanh((float(count) - inflectionValue) / diff) + 1.0f) / 2.0f;
        }

        CreatureStatModifiers const& stat = boss ? category.BossMods : category.Mods;
        mods.Health = std::max(curve * stat.Health, Settings.Min.Health);
        mods.Damage = std::max(curve * stat.Damage, Settings.Min.Damage);
        mods.Armor = std::max(curve * stat.Armor, Settings.Min.Armor);
        mods.Mana = std::max(curve * stat.Mana, Settings.Min.Mana);

        if (Settings.ShowAura)
            auraStacks = uint8(std::min<uint32>(count, 255));
        return mods;
    }

    // Kept in sync on every update, as evade and other effects remove auras.
    void UpdateAura(Creature* creature, uint8 stacks)
    {
        if (!stacks)
        {
            if (creature->HasAura(SPELL_AUTOBALANCE_AURA))
                creature->RemoveAurasDueToSpell(SPELL_AUTOBALANCE_AURA);
            return;
        }

        if (!creature->IsInWorld())
            return;

        Aura* aura = creature->GetAura(SPELL_AUTOBALANCE_AURA);
        if (!aura)
        {
            // no hotfix spell in the database, or the creature is immune
            if (!sSpellMgr->GetSpellInfo(SPELL_AUTOBALANCE_AURA))
                return;
            aura = creature->AddAura(SPELL_AUTOBALANCE_AURA, creature);
            if (!aura)
                return;
        }

        if (aura->GetStackAmount() != stacks)
            aura->SetStackAmount(stacks);

        // the effect amount is the number shown by $w1 in the tooltip (the icon shows no count for a single stack)
        if (AuraEffect* effect = aura->GetEffect(EFFECT_0))
            if (int32(effect->GetAmount()) != int32(stacks))
                effect->ChangeAmount(float(stacks));
    }

    // Only damage dealt to players and their pets/guardians is balanced, creature vs creature damage
    // (e.g. Nightmare Ichor exploding on the Eye of Il'gynoth) is not.
    float DamageModifier(Unit* target, Unit* attacker)
    {
        if (!target || !attacker || !target->GetCharmerOrOwnerPlayerOrPlayerItself())
            return 1.0f;
        Creature* creature = attacker->ToCreature();
        return creature ? creature->GetStatModifiers().Damage : 1.0f;
    }
}

class autobalance_world : public WorldScript
{
public:
    autobalance_world() : WorldScript("autobalance_world") { }

    void OnConfigLoad(bool /*reload*/) override
    {
        LoadConfig();
    }
};

class autobalance_unit : public UnitScript
{
public:
    autobalance_unit() : UnitScript("autobalance_unit") { }

    void OnCreatureUpdate(Creature* creature, uint32 /*diff*/) override
    {
        // the modifiers and the aura are only set in dungeons and raids
        if (!creature->GetMap()->IsDungeon())
            return;

        uint8 stacks = 0;
        CreatureStatModifiers mods = Calculate(creature, stacks);
        if (mods != creature->GetStatModifiers())
            creature->SetStatModifiers(mods);
        UpdateAura(creature, stacks);
    }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        float mod = DamageModifier(target, attacker);
        if (mod != 1.0f)
            damage = uint32(damage * mod);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, float& damage, SpellInfo const* /*spellInfo*/) override
    {
        damage *= DamageModifier(target, attacker);
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* /*spellInfo*/) override
    {
        float mod = DamageModifier(target, attacker);
        if (mod != 1.0f)
            damage = uint32(damage * mod);
    }
};

void Addmod_autobalanceScripts()
{
    std::string error;
    if (!sConfigMgr->LoadModuleConfig("autobalance.conf", error))
        TC_LOG_ERROR("server.loading", "[mod-autobalance] autobalance.conf not loaded, using defaults: %s", error.c_str());

    new autobalance_world();
    new autobalance_unit();
}
