/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under the MIT license, see the LICENSE file.
 */

// From SC
void AddKillStrikeScripts();

// Add all
// The core calls Add<module folder name>Scripts, with every '-' of the
// folder name replaced by '_'
void Addmod_killstrikeScripts()
{
    AddKillStrikeScripts();
}
