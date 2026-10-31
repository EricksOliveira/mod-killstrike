-- An empty `string` turns that message off, except 14, 15, 21 and 22: a reward
-- that does not fit in the bags is still mailed, with a blank subject or body.
-- 2:     {} = killer, {} = ended streak, {} = victim
-- 6-11:  {} = killer, {} = victim, {} = streak
-- 12,15: {} = streak
-- 16:    put before every chat line of this module, not before the text in the
--        middle of the screen or the mail
-- 17,18: {} = player with the bounty, {} = streak, {} = location
-- 19:    {} = hunter, {} = player with the bounty
-- 20,22: {} = player with the bounty
-- 23:    {} = player, {} = enemies killed, {} = location
-- 24:    {} = player who launched the nuke
DELETE FROM `module_string` WHERE `module` = 'mod-killstrike' AND `id` BETWEEN 0 AND 24;
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
('mod-killstrike', 11, '|cFFFFFF00TRIPLE KILL!|r |cFF81CF42{}|r has killed |cFF81CFCF{}|r and is on a {} kill streak!'),
('mod-killstrike', 12, '|cFF00FF00You earned a reward for your {} kill streak!|r'),
('mod-killstrike', 13, '|cFFFFFF00Not all of your reward fit in your bags: the rest was sent to your mailbox.|r'),
('mod-killstrike', 14, 'Kill streak reward'),
('mod-killstrike', 15, 'Your reward for a {} kill streak.'),
('mod-killstrike', 16, '|cFFFF8000[System KillStrike]:|r '),
('mod-killstrike', 17, '|cFFA335EEBOUNTY!|r |cFF81CF42{}|r is on a {} kill streak and was last seen in |cFFFFFF00{}|r. Kill them to claim the bounty!'),
('mod-killstrike', 18, '|cFFA335EEBounty:|r |cFF81CF42{}|r is still on a {} kill streak, last seen in |cFFFFFF00{}|r.'),
('mod-killstrike', 19, '|cFF81CF42{}|r has claimed the bounty on |cFF81CFCF{}|r!'),
('mod-killstrike', 20, '|cFF00FF00You claimed the bounty on {}!|r'),
('mod-killstrike', 21, 'Bounty reward'),
('mod-killstrike', 22, 'Your reward for the bounty on {}.'),
('mod-killstrike', 23, '|cFFFF0000TACTICAL NUKE!|r |cFF81CF42{}|r wiped out {} enemies in |cFFFFFF00{}|r!'),
('mod-killstrike', 24, '|cFFFF0000You were caught in the tactical nuke of {}!|r');
