/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DataMap.h"
#include "Formulas.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"
#include <algorithm>
#include <array>

static constexpr char const* KILLSTRIKE_MODULE = "mod-killstrike";
static constexpr char const* KILLSTRIKE_DATA_KEY = "mod-killstrike.Streak";

// Ids of this module's rows in the world DB `module_string` table
enum KillStrikeString : uint32
{
    KILLSTRIKE_STRING_REVENGE_EARNED  = 0,
    KILLSTRIKE_STRING_REVENGED        = 1,
    KILLSTRIKE_STRING_STREAK_ENDED_BY = 2,
    KILLSTRIKE_STRING_STREAK_ENDED    = 3,
    KILLSTRIKE_STRING_KILL            = 4,
    KILLSTRIKE_STRING_ON_STREAK       = 5,
    KILLSTRIKE_STRING_MILESTONE_5     = 6,
    KILLSTRIKE_STRING_MILESTONE_10    = 7,
    KILLSTRIKE_STRING_MILESTONE_20    = 8,
    KILLSTRIKE_STRING_MILESTONE_30    = 9,
    KILLSTRIKE_STRING_MILESTONE_50    = 10,
    KILLSTRIKE_STRING_MILESTONE_3     = 11,
};

enum KillStrikeSpell
{
    // Visual only, no buff icon: the blood orb the Blood Prince Council
    // carries out of combat (Invocation of Blood)
    SPELL_KILLSTRIKE_BLOOD_ORB = 70934,
};

struct KillStrikeMilestone
{
    uint32 Kills;
    KillStrikeString Announce;
};

// The streak keeps counting after the last milestone, with no more
// announcements
static constexpr std::array<KillStrikeMilestone, 6> KillStrikeMilestones =
{{
    {  3, KILLSTRIKE_STRING_MILESTONE_3  },
    {  5, KILLSTRIKE_STRING_MILESTONE_5  },
    { 10, KILLSTRIKE_STRING_MILESTONE_10 },
    { 20, KILLSTRIKE_STRING_MILESTONE_20 },
    { 30, KILLSTRIKE_STRING_MILESTONE_30 },
    { 50, KILLSTRIKE_STRING_MILESTONE_50 },
}};

// Filled by Load() on startup and on every config reload
struct KillStrikeConfig
{
    bool Enabled = true;
    bool Battlegrounds = true;
    bool Arenas = false;
    bool AnnounceMilestones = true;
    bool AnnounceStreakEnd = true;
    bool CheckSameIp = true;
    bool CheckGrayLevel = true;

    uint32 StreakEndMinStreak = 10;
    uint32 AuraSpellId = SPELL_KILLSTRIKE_BLOOD_ORB;
    uint32 AuraMinStreak = 30;

    void Load()
    {
        Enabled = sConfigMgr->GetOption<bool>("KillStrike.Enable", true);
        Battlegrounds =
            sConfigMgr->GetOption<bool>("KillStrike.Battlegrounds", true);
        Arenas = sConfigMgr->GetOption<bool>("KillStrike.Arenas", false);
        AnnounceMilestones =
            sConfigMgr->GetOption<bool>("KillStrike.AnnounceMilestones", true);
        AnnounceStreakEnd =
            sConfigMgr->GetOption<bool>("KillStrike.AnnounceStreakEnd", true);
        CheckSameIp =
            sConfigMgr->GetOption<bool>("KillStrike.CheckSameIp", true);
        CheckGrayLevel =
            sConfigMgr->GetOption<bool>("KillStrike.CheckGrayLevel", true);

        StreakEndMinStreak =
            sConfigMgr->GetOption<uint32>("KillStrike.StreakEnd.MinStreak", 10);
        AuraSpellId = sConfigMgr->GetOption<uint32>("KillStrike.Aura.SpellId",
            SPELL_KILLSTRIKE_BLOOD_ORB);
        AuraMinStreak =
            sConfigMgr->GetOption<uint32>("KillStrike.Aura.MinStreak", 30);
    }
};

static KillStrikeConfig killStrikeConfig;

// Stored on the Player object, so a streak is lost on logout
struct KillStrikeData : public DataMap::Base
{
    uint32 Streak = 0;
    ObjectGuid LastVictim;
};

// Text of a module string in the player's locale. Empty when the row is
// missing, or when its text was emptied to turn that message off.
template<typename... Args>
static std::string GetText(Player* player, KillStrikeString id, Args&&... args)
{
    // The locale lookup below returns an invalid pointer for a missing row
    if (!sObjectMgr->GetModuleString(KILLSTRIKE_MODULE, id))
        return "";

    return Acore::StringFormat(
        *player->GetSession()->GetModuleString(KILLSTRIKE_MODULE, id), args...);
}

template<typename... Args>
static void SendChatText(Player* player, KillStrikeString id, Args&&... args)
{
    std::string const text = GetText(player, id, args...);
    if (!text.empty())
        ChatHandler(player->GetSession()).SendSysMessage(text);
}

// Chat line plus the same text in the middle of the screen
static void SendScreenText(Player* player, KillStrikeString id)
{
    std::string const text = GetText(player, id);
    if (text.empty())
        return;

    ChatHandler(player->GetSession()).SendSysMessage(text);
    player->GetSession()->SendAreaTriggerMessage(text);
}

template<typename... Args>
static void AnnounceToServer(KillStrikeString id, Args&&... args)
{
    sWorldSessionMgr->DoForAllOnlinePlayers([&](Player* player)
    {
        SendChatText(player, id, args...);
    });
}

// Shows the aura while the streak is high enough and removes it otherwise
static void UpdateStreakAura(Player* player, uint32 streak)
{
    uint32 const spellId = killStrikeConfig.AuraSpellId;
    if (!spellId)
        return;

    if (!killStrikeConfig.Enabled || !streak
        || streak < killStrikeConfig.AuraMinStreak)
    {
        player->RemoveAurasDueToSpell(spellId);
        return;
    }

    if (!sSpellMgr->GetSpellInfo(spellId))
    {
        LOG_ERROR("module", "KillStrike.Aura.SpellId: spell {} does not "
            "exist, no aura was applied.", spellId);
        return;
    }

    // Does nothing on a dead player: OnPlayerResurrect tries again
    if (!player->HasAura(spellId))
        player->AddAura(spellId, player);
}

static uint32 GetStreak(Player* player)
{
    KillStrikeData const* data =
        player->CustomData.Get<KillStrikeData>(KILLSTRIKE_DATA_KEY);
    return data ? data->Streak : 0;
}

// Only a credited kill raises the streak. These checks stop two characters
// from feeding each other kills.
static bool IsCreditedKill(Player* killer, Player* killed,
    KillStrikeData const* killerData)
{
    if (killerData->LastVictim == killed->GetGUID())
        return false;

    std::string const& killerIp = killer->GetSession()->GetRemoteAddress();
    std::string const& victimIp = killed->GetSession()->GetRemoteAddress();
    if (killStrikeConfig.CheckSameIp && killerIp == victimIp)
        return false;

    uint8 const grayLevel = Acore::XP::GetGrayLevel(killer->GetLevel());
    if (killStrikeConfig.CheckGrayLevel && killed->GetLevel() <= grayLevel)
        return false;

    return true;
}

static void HandlePvPKill(Player* killer, Player* killed)
{
    if (!killStrikeConfig.Enabled || killer == killed)
        return;

    Map const* map = killed->GetMap();
    if (map->IsBattleArena() && !killStrikeConfig.Arenas)
        return;

    if (map->IsBattleground() && !killStrikeConfig.Battlegrounds)
        return;

    KillStrikeData* killerData =
        killer->CustomData.GetDefault<KillStrikeData>(KILLSTRIKE_DATA_KEY);
    KillStrikeData* victimData =
        killed->CustomData.GetDefault<KillStrikeData>(KILLSTRIKE_DATA_KEY);

    bool const credited = IsCreditedKill(killer, killed, killerData);
    bool const revenge = victimData->LastVictim == killer->GetGUID();
    uint32 const endedStreak = victimData->Streak;

    // A death to another player always ends the streak, also when the kill
    // is not credited to the killer
    victimData->Streak = 0;
    victimData->LastVictim.Clear();
    UpdateStreakAura(killed, 0);

    bool const streakEnded =
        endedStreak && endedStreak >= killStrikeConfig.StreakEndMinStreak;
    if (streakEnded)
    {
        if (killStrikeConfig.AnnounceStreakEnd)
            AnnounceToServer(KILLSTRIKE_STRING_STREAK_ENDED_BY,
                killer->GetName(), endedStreak, killed->GetName());

        SendScreenText(killed, KILLSTRIKE_STRING_STREAK_ENDED);
    }

    if (!credited)
        return;

    if (revenge)
    {
        SendChatText(killer, KILLSTRIKE_STRING_REVENGE_EARNED);
        SendChatText(killed, KILLSTRIKE_STRING_REVENGED);
    }

    ++killerData->Streak;
    killerData->LastVictim = killed->GetGUID();
    UpdateStreakAura(killer, killerData->Streak);

    auto const milestone = std::find_if(KillStrikeMilestones.begin(),
        KillStrikeMilestones.end(), [&](KillStrikeMilestone const& entry)
    {
        return entry.Kills == killerData->Streak;
    });

    if (milestone == KillStrikeMilestones.end())
    {
        SendScreenText(killer, KILLSTRIKE_STRING_KILL);
        return;
    }

    if (killStrikeConfig.AnnounceMilestones)
        AnnounceToServer(milestone->Announce, killer->GetName(),
            killed->GetName(), milestone->Kills);

    SendScreenText(killer, KILLSTRIKE_STRING_ON_STREAK);
}

class KillStrikePlayerScript : public PlayerScript
{
public:
    KillStrikePlayerScript() : PlayerScript("KillStrikePlayerScript", {
        PLAYERHOOK_ON_PVP_KILL,
        PLAYERHOOK_ON_PLAYER_KILLED_BY_CREATURE,
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_BEFORE_LOGOUT,
        PLAYERHOOK_ON_PLAYER_RESURRECT
    }) { }

    void OnPlayerPVPKill(Player* killer, Player* killed) override
    {
        HandlePvPKill(killer, killed);
    }

    // A killing blow from a pet, guardian or totem counts for its owner
    void OnPlayerKilledByCreature(Creature* killer, Player* killed) override
    {
        if (Player* owner = killer->GetCharmerOrOwnerPlayerOrPlayerItself())
            HandlePvPKill(owner, killed);
    }

    // The streak does not survive a logout, so its aura must not be saved.
    // The login pass cleans up an aura that was saved before a crash.
    void OnPlayerLogin(Player* player) override
    {
        UpdateStreakAura(player, GetStreak(player));
    }

    void OnPlayerBeforeLogout(Player* player) override
    {
        UpdateStreakAura(player, 0);
    }

    // Puts back an aura that was lost on death or removed by the player
    void OnPlayerResurrect(Player* player, float /*restorePercent*/,
        bool& /*applySickness*/) override
    {
        UpdateStreakAura(player, GetStreak(player));
    }
};

class KillStrikeWorldScript : public WorldScript
{
public:
    KillStrikeWorldScript() : WorldScript("KillStrikeWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD
    }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        killStrikeConfig.Load();
    }
};

void AddKillStrikeScripts()
{
    new KillStrikePlayerScript();
    new KillStrikeWorldScript();
}
