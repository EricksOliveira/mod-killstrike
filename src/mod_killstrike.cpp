/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under the MIT license, see the LICENSE file.
 */

#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DataMap.h"
#include "DatabaseEnv.h"
#include "Formulas.h"
#include "Item.h"
#include "Log.h"
#include "Mail.h"
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
#include <vector>

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
    KILLSTRIKE_STRING_REWARD          = 12,
    KILLSTRIKE_STRING_REWARD_MAILED   = 13,
    KILLSTRIKE_STRING_REWARD_SUBJECT  = 14,
    KILLSTRIKE_STRING_REWARD_BODY     = 15,
    KILLSTRIKE_STRING_CHAT_PREFIX     = 16,
};

// Sender of the mail that carries a reward that did not fit in the bags
static constexpr uint32 NPC_THE_POSTMASTER = 34337;

enum KillStrikeSpell
{
    // Invocation of Blood, the blood orb the Blood Prince Council carries out
    // of combat. No buff icon and no gameplay effect: its only effect casts
    // the cosmetic spell 71582 near its bearer every 500 ms.
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
    bool CheckSameVictim = true;

    uint32 StreakEndMinStreak = 10;
    uint32 AuraSpellId = SPELL_KILLSTRIKE_BLOOD_ORB;
    uint32 AuraMinStreak = 30;

    uint32 RewardStreak = 30;
    uint32 RewardItemId = 49426;
    uint32 RewardCount = 1;

    // Values AuraSpellId had before a reload changed it. A reload cannot reach
    // a player on a loading screen, so these are removed from each player the
    // next time that player's aura is updated.
    std::vector<uint32> RetiredAuraSpellIds;

    void RetireAuraSpell(uint32 oldSpellId)
    {
        std::erase(RetiredAuraSpellIds, AuraSpellId);

        if (oldSpellId && oldSpellId != AuraSpellId)
            RetiredAuraSpellIds.push_back(oldSpellId);
    }

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
        CheckSameVictim =
            sConfigMgr->GetOption<bool>("KillStrike.CheckSameVictim", true);

        StreakEndMinStreak =
            sConfigMgr->GetOption<uint32>("KillStrike.StreakEnd.MinStreak", 10);
        AuraSpellId = sConfigMgr->GetOption<uint32>("KillStrike.Aura.SpellId",
            SPELL_KILLSTRIKE_BLOOD_ORB);
        AuraMinStreak =
            sConfigMgr->GetOption<uint32>("KillStrike.Aura.MinStreak", 30);

        RewardStreak =
            sConfigMgr->GetOption<uint32>("KillStrike.Reward.Streak", 30);
        RewardItemId =
            sConfigMgr->GetOption<uint32>("KillStrike.Reward.ItemId", 49426);
        RewardCount =
            sConfigMgr->GetOption<uint32>("KillStrike.Reward.Count", 1);
    }
};

static KillStrikeConfig killStrikeConfig;

// Stored on the Player object, so a streak is lost on logout
struct KillStrikeData : public DataMap::Base
{
    uint32 Streak = 0;
    ObjectGuid LastVictim;
    ObjectGuid LastKiller;
};

// Text of a module string in the player's locale. Empty when the row is
// missing, or when its text was emptied to turn that message off.
template<typename... Args>
static std::string GetText(Player* player, KillStrikeString id, Args&&... args)
{
    // The locale lookup below returns an invalid pointer for a missing row
    // and for a row whose text is empty
    ModuleString const* row =
        sObjectMgr->GetModuleString(KILLSTRIKE_MODULE, id);
    if (!row || row->Content.empty())
        return "";

    return Acore::StringFormat(
        *player->GetSession()->GetModuleString(KILLSTRIKE_MODULE, id), args...);
}

// Chat lines start with the prefix, so players see which system sent them
static void SendChatLine(Player* player, std::string const& text)
{
    ChatHandler(player->GetSession()).SendSysMessage(
        GetText(player, KILLSTRIKE_STRING_CHAT_PREFIX) + text);
}

template<typename... Args>
static void SendChatText(Player* player, KillStrikeString id, Args&&... args)
{
    std::string const text = GetText(player, id, args...);
    if (!text.empty())
        SendChatLine(player, text);
}

// Chat line plus the same text, with no prefix, in the middle of the screen
static void SendScreenText(Player* player, KillStrikeString id)
{
    std::string const text = GetText(player, id);
    if (text.empty())
        return;

    SendChatLine(player, text);
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
    for (uint32 retiredSpellId : killStrikeConfig.RetiredAuraSpellIds)
        player->RemoveAurasDueToSpell(retiredSpellId);

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

// Sends the given amount of the reward item by mail, split in stacks and in
// as many mails as needed
static void MailStreakReward(Player* player, ItemTemplate const* proto,
    uint32 count, uint32 streak)
{
    std::string const subject =
        GetText(player, KILLSTRIKE_STRING_REWARD_SUBJECT);
    std::string const body =
        GetText(player, KILLSTRIKE_STRING_REWARD_BODY, streak);
    uint32 const maxStack = proto->GetMaxStackSize();

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    while (count)
    {
        MailDraft draft(subject, body);

        for (uint8 i = 0; i < MAX_MAIL_ITEMS && count; ++i)
        {
            uint32 const stack = std::min(count, maxStack);
            count -= stack;

            if (Item* item = Item::CreateItem(proto->ItemId, stack, player))
            {
                // Saved so the item is not lost on the next mail load
                item->SaveToDB(trans);
                draft.AddItem(item);
            }
        }

        draft.SendMailTo(trans, MailReceiver(player),
            MailSender(MAIL_CREATURE, NPC_THE_POSTMASTER));
    }

    CharacterDatabase.CommitTransaction(trans);
}

// Puts the reward in the bags and mails what does not fit
static void GiveStreakReward(Player* player, uint32 streak)
{
    uint32 const itemId = killStrikeConfig.RewardItemId;
    uint32 count = killStrikeConfig.RewardCount;
    if (!itemId || !count)
        return;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
    {
        LOG_ERROR("module", "KillStrike.Reward.ItemId: item {} does not "
            "exist, no reward was given.", itemId);
        return;
    }

    // Copies over the item's unique or limit category cap are not given:
    // the player could not take them out of the mail either
    uint32 overCapCount = 0;
    if (player->CanTakeMoreSimilarItems(itemId, count, nullptr,
        &overCapCount) != EQUIP_ERR_OK)
    {
        uint32 const notGiven = std::min(overCapCount, count);
        LOG_WARN("module", "KillStrike.Reward.ItemId: player {} cannot carry "
            "{} more of item {}, they were not given.",
            player->GetGUID().ToString(), notGiven, itemId);
        count -= notGiven;
    }

    if (!count)
        return;

    SendChatText(player, KILLSTRIKE_STRING_REWARD, streak);

    uint32 noSpaceCount = 0;
    ItemPosCountVec dest;
    InventoryResult const result = player->CanStoreNewItem(NULL_BAG,
        NULL_SLOT, dest, itemId, count, &noSpaceCount);

    uint32 stored = 0;
    if (!dest.empty())
    {
        uint32 const storable = result == EQUIP_ERR_OK
            ? count : count - std::min(noSpaceCount, count);

        if (Item* item = player->StoreNewItem(dest, itemId, true))
        {
            player->SendNewItem(item, storable, true, false);
            stored = storable;
        }
    }

    if (stored == count)
        return;

    MailStreakReward(player, proto, count - stored, streak);
    SendChatText(player, KILLSTRIKE_STRING_REWARD_MAILED);
}

// Only a credited kill raises the streak. These checks stop two characters
// from feeding each other kills.
static bool IsCreditedKill(Player* killer, Player* killed,
    KillStrikeData const* killerData)
{
    if (killStrikeConfig.CheckSameVictim
        && killerData->LastVictim == killed->GetGUID())
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

// Friendly fire is not a PvP kill. A mind-controlled victim is back on its own
// side by the time its death is reported, so the charm itself cannot be
// checked: allies killing it must not count.
static bool AreEnemies(Player* killer, Player* killed)
{
    if (killer->IsInSameRaidWith(killed))
        return false;

    // The battleground or arena team while inside one, the faction otherwise
    if (killer->GetBgTeamId() != killed->GetBgTeamId())
        return true;

    return killer->IsFFAPvP() && killed->IsFFAPvP();
}

static void HandlePvPKill(Player* killer, Player* killed)
{
    if (!killStrikeConfig.Enabled || !AreEnemies(killer, killed))
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
    bool const revenge = killerData->LastKiller == killed->GetGUID();
    uint32 const endedStreak = victimData->Streak;

    // A death to another player always ends the streak, also when the kill
    // is not credited to the killer
    victimData->Streak = 0;
    victimData->LastVictim.Clear();
    victimData->LastKiller = killer->GetGUID();
    UpdateStreakAura(killed, 0);

    if (killStrikeConfig.AnnounceStreakEnd && endedStreak
        && endedStreak >= killStrikeConfig.StreakEndMinStreak)
        AnnounceToServer(KILLSTRIKE_STRING_STREAK_ENDED_BY, killer->GetName(),
            endedStreak, killed->GetName());

    // The victim is told about any streak that reached the first milestone
    if (endedStreak >= KillStrikeMilestones.front().Kills)
        SendScreenText(killed, KILLSTRIKE_STRING_STREAK_ENDED);

    if (!credited)
        return;

    if (revenge)
    {
        killerData->LastKiller.Clear();
        SendChatText(killer, KILLSTRIKE_STRING_REVENGE_EARNED);
        SendChatText(killed, KILLSTRIKE_STRING_REVENGED);
    }

    ++killerData->Streak;
    killerData->LastVictim = killed->GetGUID();
    UpdateStreakAura(killer, killerData->Streak);

    if (killerData->Streak == killStrikeConfig.RewardStreak)
        GiveStreakReward(killer, killerData->Streak);

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

// The player a killing blow counts for. A pet, guardian or totem kills for its
// owner and a mind-controlled unit for the player controlling it. Nobody is
// credited while a creature is the one in control, as with a boss mind control.
static Player* GetCreditedKiller(Unit* killer)
{
    ObjectGuid const charmerGuid = killer->GetCharmerGUID();
    if (charmerGuid && !charmerGuid.IsPlayer())
        return nullptr;

    return killer->GetCharmerOrOwnerPlayerOrPlayerItself();
}

class KillStrikePlayerScript : public PlayerScript
{
public:
    KillStrikePlayerScript() : PlayerScript("KillStrikePlayerScript", {
        PLAYERHOOK_ON_PVP_KILL,
        PLAYERHOOK_ON_PLAYER_KILLED_BY_CREATURE,
        PLAYERHOOK_ON_BEFORE_LOGOUT,
        PLAYERHOOK_ON_PLAYER_RESURRECT,
        PLAYERHOOK_ON_MAP_CHANGED
    }) { }

    void OnPlayerPVPKill(Player* killer, Player* killed) override
    {
        if (Player* credited = GetCreditedKiller(killer))
            HandlePvPKill(credited, killed);
    }

    void OnPlayerKilledByCreature(Creature* killer, Player* killed) override
    {
        if (Player* credited = GetCreditedKiller(killer))
            HandlePvPKill(credited, killed);
    }

    // The streak does not survive a logout, so its aura must not be saved.
    // The streak is dropped too: a logout inside a battleground still changes
    // map and may resurrect, and those hooks would put the aura back.
    void OnPlayerBeforeLogout(Player* player) override
    {
        player->CustomData.Erase(KILLSTRIKE_DATA_KEY);
        UpdateStreakAura(player, 0);
    }

    // Puts back an aura that was lost on death or removed by the player
    void OnPlayerResurrect(Player* player, float /*restorePercent*/,
        bool& /*applySickness*/) override
    {
        UpdateStreakAura(player, GetStreak(player));
    }

    // Entering an arena strips the aura: this puts it back on the way out.
    // A login also comes through here, with the saved auras loaded and no
    // streak yet, which removes an aura that still reached the characters DB.
    void OnPlayerMapChanged(Player* player) override
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

    void OnAfterConfigLoad(bool reload) override
    {
        uint32 const oldSpellId = killStrikeConfig.AuraSpellId;

        killStrikeConfig.Load();

        if (!reload)
            return;

        killStrikeConfig.RetireAuraSpell(oldSpellId);

        // The aura options may have changed: apply them to everyone online
        sWorldSessionMgr->DoForAllOnlinePlayers([](Player* player)
        {
            UpdateStreakAura(player, GetStreak(player));
        });
    }
};

void AddKillStrikeScripts()
{
    new KillStrikePlayerScript();
    new KillStrikeWorldScript();
}
