<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
<!-- Copyright (C) 2026 DjusikW <https://github.com/DjusikW> -->

# mod-autobalance

Scales the creatures of dungeons and raids to the actual number of players inside (GMs with
`.gm on` are not counted), like AzerothCore
[mod-autobalance](https://github.com/azerothcore/mod-autobalance) without level scaling.
Health, armor, mana and the damage the creatures deal to players and their pets (melee, spells,
periodic) follow a tanh curve of the player count; creature vs creature damage is not changed.

- Settings: `conf/autobalance.conf.dist` (off by default). Copy it to `autobalance.conf` next to
  `worldserver.conf` to change values; `.reload config` applies them at once.
- Visible aura 900100 "Автобаланс" with the player count as stack count: apply
  `sql/hotfixes/2026_11_14_00_autobalance_aura.sql` to the hotfixes database
  (`AutoBalance.ShowAura = 0` hides it).
- Core hooks used: `UnitScript` (`OnCreatureUpdate` and the damage hooks) and
  `Creature::SetStatModifiers`.

License: GNU AGPL v3 or later, see `LICENSE`.
