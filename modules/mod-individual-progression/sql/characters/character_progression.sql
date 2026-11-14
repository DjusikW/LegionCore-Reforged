-- mod-individual-progression: stage of each character. Created by the module at startup.
CREATE TABLE IF NOT EXISTS `character_progression` (
  `guid` BIGINT UNSIGNED NOT NULL,
  `stage` TINYINT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
