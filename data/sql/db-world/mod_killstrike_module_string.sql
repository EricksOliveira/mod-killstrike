-- An empty `string` turns that message off.
-- 2:    {} = killer, {} = ended streak, {} = victim
-- 6-11: {} = killer, {} = victim, {} = streak
DELETE FROM `module_string` WHERE `module` = 'mod-killstrike' AND `id` IN (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-killstrike', 0, '|cFF00FF00You got revenge!|r'),
('mod-killstrike', 1, '|cFFFF0000Your enemy got revenge!|r'),
('mod-killstrike', 2, '|cFF81CF42{}|r has ended the {} man killstreak of |cFF81CFCF{}|r!'),
('mod-killstrike', 3, '|cFFFF0000Your killstreak has ended!|r'),
('mod-killstrike', 4, '|cFF00FF00You have killed an enemy player!|r'),
('mod-killstrike', 5, '|cFF00FF00You are on a killing streak!|r'),
('mod-killstrike', 6, '|cFFFFC000ULTRA KILL!|r |cFF81CF42{}|r has killed |cFF81CFCF{}|r and is on a {} kill streak!'),
('mod-killstrike', 7, '|cFFFF8000RAMPAGE!|r |cFF81CF42{}|r has killed |cFF81CFCF{}|r and is on a {} kill streak!'),
('mod-killstrike', 8, '|cFFFF4000UNSTOPPABLE!|r |cFF81CF42{}|r has killed |cFF81CFCF{}|r and is on a {} kill streak!'),
('mod-killstrike', 9, '|cFFFF0000MASSACRE!|r |cFF81CF42{}|r has killed |cFF81CFCF{}|r and is on a {} kill streak!'),
('mod-killstrike', 10, '|cFFA335EEGODLIKE!|r |cFF81CF42{}|r has killed |cFF81CFCF{}|r and is on a {} kill streak!'),
('mod-killstrike', 11, '|cFFFFFF00TRIPLE KILL!|r |cFF81CF42{}|r has killed |cFF81CFCF{}|r and is on a {} kill streak!');
