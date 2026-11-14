-- Emerald Nightmare: Drake's Fury (113203, invisible stalkers under the Nightmare Drakes) cast their
-- periodic "Drake's Wrath" aura 225324 only once after spawn (event_flags 1 = NOT_REPEATABLE),
-- so the ground strikes stopped shortly after the instance loaded. Repeat every 5-6 s.
UPDATE smart_scripts SET event_flags = 0 WHERE entryorguid = 113203 AND source_type = 0 AND id = 0;
-- Emerald Nightmare: Elerethe Renferal platform flights (paths 10608700-02) were walk (move_type 0) in the air.
UPDATE waypoint_data_script SET move_type = 1 WHERE id BETWEEN 10608700 AND 10608702;
-- Emerald Nightmare: quest 44283 (Malfurion, 4x Essence of Clarity 141303) - the essences drop from
-- Cenarius' chest "Spoils of the Nightmare" (254168, loot 67803) per wowhead; the drop was missing.
DELETE FROM gameobject_loot_template WHERE Entry = 67803 AND Item = 141303;
INSERT INTO gameobject_loot_template (Entry, Item, Reference, Chance, QuestRequired, LootMode, GroupId, MinCount, MaxCount, Comment)
VALUES (67803, 141303, 0, 100, 1, 1, 0, 4, 4, 'Essence of Clarity - quest 44283');
-- Emerald Nightmare (Xavius): Shadow Dweller 111370 randomly picked the invisible trigger model 11686.
UPDATE creature_template_wdb SET Displayid2 = 0 WHERE Entry = 111370 AND Displayid2 = 11686;
-- Emerald Nightmare: Nightmare Drakes (111567, decorative, ImmuneToPC/NPC) patrol 1-5 yd above the slope;
-- on retail they fly in the sky above the Drake's Fury strikes. Raise spawns and their GUID-keyed paths by 25 yd.
-- Revert: same statements with -25.
UPDATE waypoint_data w JOIN creature c ON c.guid = w.id AND c.id = 111567 AND c.map = 1520
   SET w.position_z = w.position_z + 25;
UPDATE creature SET position_z = position_z + 25 WHERE id = 111567 AND map = 1520;
