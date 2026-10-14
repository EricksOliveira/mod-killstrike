# mod-killstrike

PvP kill streak system for AzerothCore (3.3.5a).

Every kill of an enemy player raises the killer's streak. At 3, 5, 10, 20, 30
and 50 kills the whole server is told about it. Being killed by an enemy player
ends the streak.

## Rules

- The 3, 5, 10, 20, 30 and 50 kill milestones are announced to the server. The
  streak keeps counting after 50, with no more milestones.
- Killing a player with a streak of 10 or more is announced to the server.
- From 30 kills on, the player carries a visible aura (by default the blood
  orb of the Blood Prince Council) until the streak ends.
- At 30 kills the player gets a reward, by default 1 Emblem of Frost. The
  streak, the item and the amount are set in the config
  (`KillStrike.Reward.*`). It is given once per streak, and what does not fit
  in the bags is sent to the mailbox. Copies the player cannot carry, such as
  a second copy of a unique item, are not given; copies still waiting in the
  mailbox are not counted.
- Killing the player who killed you last shows a "revenge" message to both.
- Only kills between enemies count: players of different factions (or of
  different teams inside a battleground or arena), and players of the same
  faction when both are flagged for free-for-all PvP, as in a free-for-all
  area or on a free-for-all realm. Members of the same group or raid are never
  enemies. Any other kill is ignored for both players: the killer's streak does
  not rise and the victim keeps its own streak and aura.
- Killing blows from pets, guardians and totems count for their owner. A kill
  by a mind-controlled player counts for the player controlling it, and for
  nobody when a creature is in control.
- The streak is only kept in memory: it is lost on logout and on a server
  restart. Only a death to an enemy player ends it, a death to a creature does
  not.

A kill between enemies does not raise the killer's streak when:

- the victim is the same player as the killer's previous kill
  (`KillStrike.CheckSameVictim`);
- killer and victim share the same IP address (`KillStrike.CheckSameIp`);
- the victim's level is gray to the killer (`KillStrike.CheckGrayLevel`).

The victim's own streak still ends in these cases.

## Installation

1. Place the module in the `modules/` folder of your AzerothCore source.
2. Re-run CMake and build.
3. Copy `mod_killstrike.conf.dist` to `mod_killstrike.conf` in your config
   folder and edit it. The announcements, the aura, the reward and each
   check can be changed or turned off there.
4. The SQL in `data/sql/db-world/` is applied by the database updater on the
   next worldserver start.

## Messages

All texts are in the world database table `module_string`, under the module
name `mod-killstrike`. Edit a row to change a text, or empty it to turn that
message off, then run `.reload module_string`. Rows 14 and 15 cannot be turned
off: a reward that does not fit in the bags is still mailed, with a blank
subject or body when its row is empty. Translations go in
`module_string_locale`.

| id   | Message                                             |
|------|-----------------------------------------------------|
| 0    | Killer: got revenge                                 |
| 1    | Victim: the enemy got revenge                       |
| 2    | Server: a streak was ended (killer, streak, victim) |
| 3    | Victim: own streak of 3 or more ended               |
| 4    | Killer: normal kill                                 |
| 5    | Killer: reached a milestone                         |
| 6-10 | Server: milestone 5, 10, 20, 30, 50 (killer, victim, streak) |
| 11   | Server: milestone 3 (killer, victim, streak)        |
| 12   | Killer: earned the reward (streak)                  |
| 13   | Killer: part of the reward was mailed               |
| 14   | Mail subject of a mailed reward                     |
| 15   | Mail body of a mailed reward (streak)               |
| 16   | Prefix put before every chat line of this module    |
