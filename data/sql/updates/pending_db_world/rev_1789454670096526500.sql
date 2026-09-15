-- Vanilla-Plus X19: wire spell_troll_berserking (spell_generic.cpp) to spell 26297
-- (Troll racial Berserking, the only 1.12 class-gated id [20554/26296/26297] that
-- survives into this fork's 3.3.5a Spell.dbc by name) so its haste amount is computed
-- from missing health at cast time instead of the DBC's flat value.
DELETE FROM `spell_script_names` WHERE `spell_id` = 26297;
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (26297, 'spell_troll_berserking');
