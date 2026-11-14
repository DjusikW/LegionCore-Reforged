-- mod-individual-progression: let the DB updater apply the module's world SQL
INSERT IGNORE INTO `updates_include` (`path`, `state`) VALUES ('$/modules/mod-individual-progression/sql/world', 'RELEASED');
