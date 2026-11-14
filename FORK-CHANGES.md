# DjusikW fork changes

**English** | [Русский](FORK-CHANGES.ru.md)

Fork of [Titans-Project/LegionCore-Reforged](https://github.com/Titans-Project/LegionCore-Reforged) for a non-commercial home WoW Legion 7.3.5 server. Branch `djusikw/main` = upstream `main` + the changes below.

Contributors: DjusikW, Claude (Anthropic) as commit co-author.

Status: **tested** — checked in game; **built** — compiles, not tested in game or tested partially.

## Core

| Change | Status |
|---|---|
| Rogue-class creatures get an energy bar. Creature energy was always 0, so bosses with energy-driven phases (Nythendra, Elerethe Renferal, Cenarius, Dragons of Nightmare and others) stayed in their first phase. | partially tested: Nythendra has energy and casts Infested Breath |
| Dungeon and raid autobalance by player count (`AutoBalance.*` in `worldserver.conf`, off by default), visible aura 900100 "AutoBalance" (`sql/updates/hotfixes/2026_11_14_00_autobalance_aura.sql`). | built |
| Playerbots: module system (`modules/<name>` with its own `.conf`), client-less bot sessions, `PlayerScript::OnWhisper`, LFG / loot roll / kick vote accessors (9 commits ported from [DemonCore](https://github.com/DemonCore-Legion/DemonCore)) and the [DemonCore-Playerbots](https://github.com/DemonCore-Legion/DemonCore-Playerbots) module `ab4f4b1` in `modules/mod-playerbots` (`playerbots.conf`, `.playerbots create <class> [spec]`). | built |

## Emerald Nightmare

| Change | Status |
|---|---|
| Script fixes for Nythendra, Il'gynoth, Elerethe Renferal, Cenarius | built, in-game testing in progress |
| Trash before Xavius — 5 waves | built |
| DB (`sql/updates/world/2026_11_14_00_emerald_nightmare.sql`): Drake's Fury strikes repeat, Elerethe flies between platforms, Essence of Clarity for quest 44283, Shadow Dweller model, Nightmare Drakes fly higher | applied to DB, testing in progress |

## Quests

| Change | Status |
|---|---|
| DB (`sql/updates/world/2026_11_14_02_quest_item_loot_from_cata.sql`): 615 quest items that dropped nowhere get their loot from the Cataclysm 4.3.4 world DB (TDB 434.22011, The Cataclysm Preservation Project): 4487 creature, 125 gameobject and 93 container loot rows; 613 creatures without a loot table point at their own entry. New rows only. Quests 1–60 with an item that can't be obtained: 1083 → 48. | applied to DB, not tested in game |
| DB (`sql/updates/world/2026_11_14_03_quest_objectives_from_cata.sql`): 16 quests that the 7.3.5 client knows had no objectives at all and could be turned in without doing anything; objectives taken from the Cataclysm 4.3.4 world DB (19 rows, custom objective IDs 16000000 + quest × 10 + index). | applied to DB, not tested in game |

## Known issues

- Emerald Nightmare: drakes before the second boss don't aggro; Elerethe web animation; Nythendra has no Tail Lash (203024).
- Trial character scenario (maps 1554/1557) is not implemented.
