-- Kill credit that names the creature itself (or repeats KillCredit1) counted every kill twice for quests:
-- Bristleback Quilboar 2952 gave 2/6 per kill in 14452 "Rite of Strength". 20 creatures.
UPDATE `creature_template_wdb` SET `KillCredit1` = 0 WHERE `KillCredit1` = `entry` AND `entry` IN (2952, 25221, 25537, 31159, 31160, 31161, 39075, 39148, 39149, 46819, 56149, 56150, 58813, 67599, 85726, 86149, 91566, 103837, 121335);
UPDATE `creature_template_wdb` SET `KillCredit2` = 0 WHERE `entry` = 27118 AND `KillCredit2` = `KillCredit1`;
UPDATE `creature_template_wdb` SET `KillCredit2` = 0 WHERE `entry` = 117126 AND `KillCredit2` = `entry`;
