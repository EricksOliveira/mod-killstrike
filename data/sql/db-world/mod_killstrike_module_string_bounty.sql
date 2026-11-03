-- An empty `string` turns that message off, except 21 and 22: a bounty reward
-- that does not fit in the bags is still mailed, with a blank subject or body.
-- 17,18: {} = player with the bounty, {} = streak, {} = location
-- 19:    {} = hunter, {} = player with the bounty
-- 20,22: {} = player with the bounty
-- 23:    {} = player, {} = enemies killed, {} = location
-- 24:    {} = player who launched the nuke
DELETE FROM `module_string` WHERE `module` = 'mod-killstrike' AND `id` BETWEEN 17 AND 24;
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-killstrike', 17, '|cFFA335EEBOUNTY!|r |cFF81CF42{}|r is on a {} kill streak and was last seen in |cFFFFFF00{}|r. Kill them to claim the bounty!'),
('mod-killstrike', 18, '|cFFA335EEBounty:|r |cFF81CF42{}|r is still on a {} kill streak, last seen in |cFFFFFF00{}|r.'),
('mod-killstrike', 19, '|cFF81CF42{}|r has claimed the bounty on |cFF81CFCF{}|r!'),
('mod-killstrike', 20, '|cFF00FF00You claimed the bounty on {}!|r'),
('mod-killstrike', 21, 'Bounty reward'),
('mod-killstrike', 22, 'Your reward for the bounty on {}.'),
('mod-killstrike', 23, '|cFFFF0000TACTICAL NUKE!|r |cFF81CF42{}|r wiped out {} enemies in |cFFFFFF00{}|r!'),
('mod-killstrike', 24, '|cFFFF0000You were caught in the tactical nuke of {}!|r');
