# mod-killstrike

PvP kill streak system for AzerothCore (3.3.5a).

Every player kill raises the killer's streak. At 3, 5, 10, 20, 30 and 50 kills
the whole server is told about it. Being killed by another player ends the
streak.

## Rules

- The 3, 5, 10, 20, 30 and 50 kill milestones are announced to the server. The
  streak keeps counting after 50, with no more milestones.
- Killing a player with a streak of 10 or more is announced to the server.
- From 30 kills on, the player carries a visible aura (by default the blood
  orb of the Blood Prince Council) until the streak ends.
- Killing the player who killed you last shows a "revenge" message to both.
- Only kills between enemies count: players of different factions (or of
  different teams inside a battleground or arena), and players of the same
  faction when both are in a free-for-all PvP area. Kills between members of
  the same group or raid never count.
- Killing blows from pets, guardians and totems count for their owner. A kill
  by a mind-controlled player counts for the player controlling it, and for
  nobody when a creature is in control.
- The streak is only kept in memory: it is lost on logout and on a server
  restart. Only a death to another player ends it, a death to a creature does
  not.

A kill does not raise the killer's streak when:

- the victim is the same player as the killer's previous kill;
- killer and victim share the same IP address (`KillStrike.CheckSameIp`);
- the victim's level is gray to the killer (`KillStrike.CheckGrayLevel`).

The victim's own streak still ends in these cases.

## Installation

1. Place the module in the `modules/` folder of your AzerothCore source.
2. Re-run CMake and build.
3. Copy `mod_killstrike.conf.dist` to `mod_killstrike.conf` in your config
   folder and edit it. The announcements, the aura and each check can be
   changed or turned off there.
4. The SQL in `data/sql/db-world/` is applied by the database updater on the
   next worldserver start.

## Messages

All texts are in the world database table `module_string`, under the module
name `mod-killstrike`. Edit a row to change a text, or empty it to turn that
message off, then run `.reload module_string`. Translations go in
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
