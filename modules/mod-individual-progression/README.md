<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
<!-- Copyright (C) 2026 DjusikW <https://github.com/DjusikW> -->

# mod-individual-progression

Personal progression through the expansions, one character at a time: every character has a stage,
and content of later stages (raids, dungeons, continents, intro quests, level) stays closed until the
character has cleared the previous one. Groups may mix stages. Idea after the AzerothCore module
[mod-individual-progression](https://github.com/ZhengPeiRu21/mod-individual-progression); written
for LegionCore-Reforged (7.3.5).

## Stages

Stage `x0` of an expansion is levelling: its zones and dungeons, up to the expansion's level cap.
Reaching the cap opens `x1` (the launch raids); the last boss of each raid tier opens the next tier
(raids and patch dungeons in release order), and the last tier opens the next expansion.

| Expansion | Stages | Level cap | Last tier ends with |
|---|---|---|---|
| Classic | 0 levelling, 1 Molten Core / Onyxia, 2 Blackwing Lair, 3 Ahn'Qiraj | 60 | C'Thun → 10 |
| Burning Crusade | 10 levelling, 11 Karazhan / Gruul / Magtheridon, 12 SSC / The Eye, 13 Hyjal / Black Temple, 14 Sunwell / Magisters' Terrace | 70 | Kil'jaeden → 20 |
| Wrath of the Lich King | 20 levelling, 21 Naxxramas / OS / EoE / VoA, 22 Ulduar, 23 Trial of the Crusader, 24 Icecrown Citadel, 25 Ruby Sanctum | 80 | Halion → 30 |
| Cataclysm | 30 levelling, 31 BWD / BoT / TotFW / Baradin Hold, 32 Firelands / Zul'Aman / Zul'Gurub, 33 Dragon Soul / End Time | 85 | Madness of Deathwing → 40 |
| Mists of Pandaria | 40 levelling, 41 MSV / HoF / ToES, 42 Throne of Thunder, 43 Siege of Orgrimmar | 90 | Garrosh → 50 |
| Warlords of Draenor | 50 levelling, 51 Highmaul, 52 Blackrock Foundry, 53 Hellfire Citadel / Tanaan | 100 | Archimonde → 60 |
| Legion | 60 levelling, 61 Emerald Nightmare, 62 Trial of Valor / Return to Karazhan, 63 Nighthold, 64 Tomb of Sargeras / Cathedral, 65 Antorus / Seat / Argus | 110 | — |

Full list with every transition boss: world table `individual_progression_stage` and
`individual_progression_boss`.

## What the module does

| | |
|---|---|
| Level cap | No experience at the cap of the stage's expansion (GM level commands are not limited). |
| Transition | The last boss of a tier dies (any killer: player, pet, script) → every player in the instance moves up (never down); outside instances, the killer's group on the map. Encounters that end without a death use the boss state: Madness of Deathwing (`individual_progression_encounter`). In Dragon Soul, Thrall after the fight also gives the stage to whoever missed it. |
| Maps | Dungeons, raids and whole continents of a later stage refuse the player with a message; the dungeon finder locks them. |
| Zones | Zones of a later stage on a shared map (Outland on the map of Eversong and Azuremyst, Cataclysm zones on the old continents) send the player back to the hearthstone. |
| Quests | Breadcrumbs of `individual_progression_quest` and quests of a later expansion above level 60 (`quest_template.Expansion`) are hidden and refused. Pandaren and death knight starting quests stay open. |
| Classes and races | Death knight from stage 10, demon hunter and allied races from stage 60 on any character of the account. A new death knight starts at stage 10. |
| Existing characters | On the first login with the module: the stage of their level (level 60 → 0, then at once 1). |
| Bots | Characters of playerbot accounts (`^PLAYERBOT`) have no stage and no limits. Alts played as bots keep their own stage. |

GMs (`.gm on`) enter every map.

## Commands

| Command | |
|---|---|
| `.progression show [$name]` | stage of the character (online or offline) |
| `.progression set $stage [$name]` | set the stage, down as well as up |

## Configuration

`individual-progression.conf.dist` (copy to `individual-progression.conf` next to `worldserver.conf`
to change values): `Enable`, `StartingStage`, `LevelCap.<0..60>`, `CustomProgression` (extra
"creature entry → stage" transitions), `DeathKnightUnlockStage`, `DeathKnightStartStage`,
`DemonHunterUnlockStage`, `AlliedRacesUnlockStage`, `ExcludedAccountsRegex`.

## Database

- `characters.character_progression (guid, stage)` — created by the module at startup.
- World data `sql/world/2026_11_14_00_individual_progression_data.sql`: stages, map / zone / quest
  → stage, final boss → next stage, encounter → next stage, Thrall's script name. The core's DB
  updater applies it through the `updates_include` path added by
  `sql/updates/world/2026_11_14_01_individual_progression_include.sql` (the first start registers
  the path, the next one applies the file). The file is generated from the client data (dungeon
  journal `JournalTierXInstance`, `Map.db2`) by a script outside the repository; edit the tables or
  the file by hand for local changes.

## Core hooks used

`PlayerScript`: `OnLogin`, `OnGiveXP`, `OnLevelChanged`, `OnUpdateZone`, `OnLogout`, `OnDelete`, and
the hooks added for this module: `OnCreatureDeath`, `OnInstanceBossDone`, `CanEnterMap` (also asked
by the dungeon finder), `CanTakeQuest`, `CanCreateCharacter`.

## Known limits

- One boss per tier: the other raids of the tier don't have to be cleared.
- Quests of the old world stay as Cataclysm made them; only intro quests are closed.
- Characters of a stage can still reach content through heirlooms, flying or other players'
  summons outside instances; the level cap and the map checks hold the progression.
