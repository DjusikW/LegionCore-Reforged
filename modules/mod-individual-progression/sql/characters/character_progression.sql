-- SPDX-License-Identifier: AGPL-3.0-or-later
-- Copyright (C) 2026 DjusikW <https://github.com/DjusikW>
-- mod-individual-progression: stage of each character. Created by the module at startup.
CREATE TABLE IF NOT EXISTS `character_progression` (
  `guid` BIGINT UNSIGNED NOT NULL,
  `stage` TINYINT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
