-- Portions Copyright (C) 2026 DjusikW <https://github.com/DjusikW>
-- Quests offered in 7.3.5 (present in QuestV2.db2) that had no objectives at all, so they could be turned in
-- without doing anything. Objectives from the Cataclysm 4.3.4 world DB (TDB 434.22011, The Cataclysm
-- Preservation Project). Objective IDs 16000000 + quest*10 + index (custom range, no retail IDs known).
DELETE FROM quest_objectives WHERE QuestID IN (31,531,1188,1898,1978,3002,3841,4733,5061,5462,7504,7505,8951,8994,24526,28893) AND ID BETWEEN 16000000 AND 16777215;
INSERT INTO quest_objectives (ID, QuestID, Type, StorageIndex, ObjectID, Amount, Flags, Flags2, TaskStep, Description, VerifiedBuild) VALUES
(16000310,31,1,0,15885,1,0,0,0,'',0),
(16005310,531,1,0,2713,1,0,0,0,'',0),
(16011880,1188,1,0,5862,1,0,0,0,'',0),
(16018980,1898,1,0,7231,1,0,0,0,'',0),
(16019780,1978,1,0,7294,1,0,0,0,'',0),
(16030020,3002,1,0,9371,1,0,0,0,'',0),
(16038410,3841,1,0,11102,1,0,0,0,'',0),
(16047330,4733,1,0,12242,1,0,0,0,'',0),
(16050610,5061,1,0,15885,1,0,0,0,'',0),
(16054620,5462,1,0,13585,1,0,0,0,'',0),
(16075040,7504,1,0,18362,1,0,0,0,'',0),
(16075050,7505,1,0,18363,1,0,0,0,'',0),
(16089510,8951,1,0,16715,1,0,0,0,'',0),
(16089511,8951,1,1,16719,1,0,0,0,'',0),
(16089512,8951,1,2,16718,1,0,0,0,'',0),
(16089940,8994,1,0,22138,40,0,0,0,'',0),
(16089941,8994,1,1,13512,1,0,0,0,'',0),
(16245260,24526,0,0,44175,2,0,0,0,'',0),
(16288930,28893,1,0,17346,1,0,0,0,'',0);
