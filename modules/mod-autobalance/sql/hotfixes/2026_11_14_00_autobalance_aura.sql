-- SPDX-License-Identifier: AGPL-3.0-or-later
-- Copyright (C) 2026 DjusikW <https://github.com/DjusikW>
-- mod-autobalance: visible aura "Автобаланс" (spell 900100) on creatures scaled by the module.
-- Apply to the hotfixes database (the core SQL updater does not read module folders).
-- The stack count shows the number of players the creature is balanced for.
-- Effect amount = stack count (base points 1), sent to the client (SPELL_ATTR8_AURA_SEND_AMOUNT) for $w1 in the tooltip.
-- Dummy aura on self, infinite, up to 255 stacks, no dispel type (cannot be dispelled or stolen),
-- icon of Rallying Cry (FileDataID 132351). Sent to the client as a hotfix.

DELETE FROM spell WHERE ID = 900100;
INSERT INTO spell (ID, Name, NameSubtext, Description, AuraDescription, VerifiedBuild) VALUES
(900100, 'Автобаланс', '', 'Сила существа подстроена под число игроков в подземелье или рейде.', 'Сила подстроена под число игроков: $w1. Чем меньше игроков, тем слабее противник.', 0);

DELETE FROM spell_locale WHERE ID = 900100;
INSERT INTO spell_locale (ID, locale, Name_lang, NameSubtext_lang, Description_lang, AuraDescription_lang, VerifiedBuild) VALUES
(900100, 'ruRU', 'Автобаланс', '', 'Сила существа подстроена под число игроков в подземелье или рейде.', 'Сила подстроена под число игроков: $w1. Чем меньше игроков, тем слабее противник.', 0);

DELETE FROM spell_misc WHERE ID = 900100 OR SpellID = 900100;
INSERT INTO spell_misc (ID, CastingTimeIndex, DurationIndex, RangeIndex, SchoolMask, SpellIconFileDataID, Speed, ActiveIconFileDataID, LaunchDelay, DifficultyID,
    Attributes1, Attributes2, Attributes3, Attributes4, Attributes5, Attributes6, Attributes7, Attributes8, Attributes9, Attributes10, Attributes11, Attributes12, Attributes13, Attributes14,
    SpellID, VerifiedBuild) VALUES
(900100, 1, 21, 1, 1, 132351, 0, 132351, 0, 0,
    -2147483648, 0, 0, 0, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 0,
    900100, 0);

DELETE FROM spell_effect WHERE ID = 900100 OR SpellID = 900100;
INSERT INTO spell_effect (ID, Effect, EffectBasePoints, EffectIndex, EffectAura, DifficultyID, ImplicitTarget1, ImplicitTarget2, SpellID, VerifiedBuild) VALUES
(900100, 6, 1, 0, 4, 0, 1, 0, 900100, 0);

DELETE FROM spell_aura_options WHERE ID = 900100 OR SpellID = 900100;
INSERT INTO spell_aura_options (ID, ProcCharges, ProcTypeMask, ProcCategoryRecovery, CumulativeAura, SpellProcsPerMinuteID, DifficultyID, ProcChance, SpellID, VerifiedBuild) VALUES
(900100, 0, 0, 0, 255, 0, 0, 0, 900100, 0);

-- hotfix_data: Spell 3776013982, SpellMisc 3322146344, SpellEffect 4030871717, SpellAuraOptions 4096770149
DELETE FROM hotfix_data WHERE Id BETWEEN 9200001 AND 9200014;
INSERT INTO hotfix_data (Id, TableHash, RecordID, Timestamp, Deleted) VALUES
(9200011, 3776013982, 900100, 0, 0),
(9200012, 3322146344, 900100, 0, 0),
(9200013, 4030871717, 900100, 0, 0),
(9200014, 4096770149, 900100, 0, 0);
