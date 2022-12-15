/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "Group.h"

bool MFKEnable = true;
bool MFKAnnounceModule = true;
bool MFKKillingBlowOnly = false;
bool MFKMoneyForNothing = false;
bool MFKLowLevelBounty = false;
uint32 MFKMinPVPLevel = 10;
uint32 MFKPVPCorpseLootPercent = 5;
uint32 MFKKillMultiplier = 100;
uint32 MFKPVPMultiplier = 200;
uint32 MFKDungeonBossMultiplier = 10;
uint32 MFKWorldBossMultiplier = 15;

// Add player scripts
class MFKConfig : public WorldScript
{
public:
    MFKConfig() : WorldScript("MFKConfig") { }

    void OnBeforeConfigLoad(bool reload) override
    {
        MFKEnable = sConfigMgr->GetOption<bool>("MFK.Enable", true);
        MFKAnnounceModule = sConfigMgr->GetOption<bool>("MFK.Announce", true);
        MFKKillingBlowOnly = sConfigMgr->GetOption<bool>("MFK.KillingBlowOnly", false);
        MFKMoneyForNothing = sConfigMgr->GetOption<bool>("MFK.MoneyForNothing", false);
        MFKLowLevelBounty = sConfigMgr->GetOption<bool>("MFK.LowLevelBounty", false);
        MFKMinPVPLevel = sConfigMgr->GetOption<uint32>("MFK.MinPVPLevel", 10);
        MFKPVPCorpseLootPercent = sConfigMgr->GetOption<uint32>("MFK.PVP.CorpseLootPercent", 5);
        MFKKillMultiplier = sConfigMgr->GetOption<uint32>("MFK.Kill.Multiplier", 100);
        MFKPVPMultiplier = sConfigMgr->GetOption<uint32>("MFK.PVP.Multiplier", 200);
        MFKDungeonBossMultiplier = sConfigMgr->GetOption<uint32>("MFK.DungeonBoss.Multiplier", 25);
        MFKWorldBossMultiplier = sConfigMgr->GetOption<uint32>("MFK.WorldBoss.Multiplier", 100);
    }
};

class MFKAnnounce : public PlayerScript
{

public:
    MFKAnnounce() : PlayerScript("MFKAnnounce") {}

    void OnLogin(Player* player)
    {
        // Announce Module
        if (MFKEnable)
        {
            if (MFKAnnounceModule)
            {
                ChatHandler(player->GetSession()).SendSysMessage("This server is running the|cff4CFF00 Muertes por Dinero |rmodule.");
            }
        }
    }
};

class MoneyForKills : public PlayerScript
{
public:
    MoneyForKills() : PlayerScript("MoneyForKills") { }

    // Player Kill Reward
    void OnPVPKill(Player* player, Player* victim)
    {
        // If enabled...
        if (MFKEnable)
        {
            // If enabled...
            if (MFKPVPMultiplier > 0)
            {
                std::ostringstream ss;

                // No hay recompensa por suicidarse
                if (player->GetName().compare(victim->GetName()) == 0)
                {
                    // Inform the world
                    ss << "|cffFFFFFF" << player->GetName() << "|cffff6060 se encontr\xA2; con una muerte prematura!";
                    sWorld->SendServerMessage(SERVER_MSG_STRING, ss.str().c_str());
                    return;
                }

                // ¿Ese pobre bastardo valía algún botín?
                if (victim->getLevel() <= MFKMinPVPLevel)
                {
                    ss << "|cffFF0000[|cffff6060PVP|cffFF0000]|cffFFFFFF " << victim->GetName()
                        << "|cffFF0000 Fue sacrificado sin piedad por|cffff6060 " << player->GetName() << "|cffFF0000 !";
                    sWorld->SendServerMessage(SERVER_MSG_STRING, ss.str().c_str());
                    return;
                }

                // Calculate the amount of gold to give to the victor
                const uint32 VictimLevel = victim->getLevel();
                const int VictimLoot = (victim->GetMoney() * MFKPVPCorpseLootPercent) / 100;    // Configured % of victim's loot
                const int BountyAmount = ((VictimLevel * MFKPVPMultiplier) / 3);                // Modifier

                // Dispara al cadáver de la víctima y comprueba si hay botín.
                // Si ellos tienen al menos 1 oro
                if (victim->GetMoney() >= 10000)
                {
                    // El jugador saquea el 5% del oro de la víctima.
                    player->ModifyMoney(VictimLoot);
                    victim->ModifyMoney(-VictimLoot);

                    // Informar al jugador del botín de cadáveres.
                    Notify(player, victim, NULL, "Loot", NULL, VictimLoot);

                    // Paga al jugador la recompensa adicional de JcJ
                    player->ModifyMoney(BountyAmount);
                }
                else
                {
                    // Paga al jugador la recompensa adicional de JcJ
                    player->ModifyMoney(BountyAmount);
                }

                // Informar al jugador del monto de la recompensa.
                Notify(player, victim, NULL, "PVP", BountyAmount, VictimLoot);

                //return;
            }
        }
    }

    // Creature Kill Reward
    void OnCreatureKill(Player* player, Creature* killed)
    {
        std::ostringstream ss;

        // No reward for killing yourself
        if (player->GetName().compare(killed->GetName()) == 0)
        {
            // Inform the world
            ss << "|cffFFFFFF" << player->GetName() << "|cffff6060 se encontró con una muerte prematura!";
            sWorld->SendServerMessage(SERVER_MSG_STRING, ss.str().c_str());
            return;
        }

        // If enabled...
        if (MFKEnable)
        {
            // Get the creature level
            const uint32 CreatureLevel = killed->getLevel();
            const uint32 CharacterLevel = player->getLevel();

            // ¿Qué mató el jugador?
            if (killed->IsDungeonBoss() || killed->isWorldBoss())
            {
                uint32 BossMultiplier;

                // Dungeon Boss or World Boss multiplier?
                if (killed->IsDungeonBoss())
                {
                    BossMultiplier = MFKDungeonBossMultiplier;
                }
                else
                {
                    BossMultiplier = MFKWorldBossMultiplier;
                }

                // If enabled...
                if (BossMultiplier > 0)
                {
                    // Reward based on creature level
                    const int BountyAmount = ((CreatureLevel * BossMultiplier) * 100);

                    if (killed->IsDungeonBoss())
                    {
                        // Pay the bounty amount
                        CreatureBounty(player, killed, "DungeonBoss", BountyAmount);
                    }
                    else
                    {
                        // Pay the bounty amount
                        CreatureBounty(player, killed, "WorldBoss", BountyAmount);
                    }
                }
            }
            else
            {
                // If enabled...
                if (MFKKillMultiplier > 0)
                {
                    // Reward based on creature level
                    int BountyAmount = ((CreatureLevel * MFKKillMultiplier) / 3);

                    // Is the character higher level than the mob?
                    if (CharacterLevel > CreatureLevel)
                    {
                        // If Low Level Bounty not enabled zero the bounty
                        if (!MFKLowLevelBounty)
                        {
                            BountyAmount = 0;
                        }
                        else
                        {
                            // Is the creature worth XP or Honor?
                            if (!player->isHonorOrXPTarget(killed))
                            {
                                // BountyAmount = 0;
                            }
                        }
                    }

                    // Pay the bounty amount
                    CreatureBounty(player, killed, "MOB", BountyAmount);
                }
            }
        }
    }

    // Pay Creature Bounty
    void CreatureBounty(Player* player, Creature* killed, std::string KillType, int bounty)
    {
        Group* group = player->GetGroup();
        Group::MemberSlotList const& members = group->GetMemberSlots();

        if (MFKEnable)
        {
            // If we actually have a bounty..
            if (bounty >= 1)
            {
                // Determine who receives the bounty
                if (!group || MFKKillingBlowOnly == 1)
                {
                    // Pay a specific player bounty amount
                    player->ModifyMoney(bounty);

                    // Inform the player of the bounty amount
                    Notify(player, NULL, killed, KillType, bounty, NULL);
                }
                else
                {
                    // Paga al grupo (OnCreatureKill solo recompensa al jugador que recibió el golpe mortal)
                    for (Group::MemberSlotList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
                    {
                        Group::MemberSlot const& slot = *itr;
                        Player* playerInGroup = ObjectAccessor::FindPlayer((*itr).guid);

                        // Pay each player in the group
                        if (playerInGroup && playerInGroup->GetSession())
                        {
                            // Money for nothing and the kills for free..
                            if (MFKMoneyForNothing == 1)
                            {
                                // Pay the bounty
                                playerInGroup->ModifyMoney(bounty);

                                // Inform the player of the bounty amount
                                Notify(playerInGroup, NULL, killed, KillType, bounty, NULL);
                            }
                            else
                            {
                                // Only pay players that are in reward distance	
                                if (playerInGroup->IsAtGroupRewardDistance(killed))
                                {
                                    // Pay the bounty
                                    playerInGroup->ModifyMoney(bounty);

                                    // Inform the player of the bounty amount
                                    Notify(playerInGroup, NULL, killed, KillType, bounty, NULL);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Payment/Kill Notification
    void Notify(Player* player, Player* victim, Creature* killed, std::string KillType, int bounty, int loot)
    {
        std::ostringstream ss;
        std::ostringstream sv;
        int result[3];

        if (MFKEnable)
        {
            // Determine type of kill
            if (KillType == "Loot")
            {
                const int copper = loot % 100;
                loot = (loot - copper) / 100;
                const int silver = loot % 100;
                const int gold = (loot - silver) / 100;
                result[0] = copper;
                result[1] = silver;
                result[2] = gold;
            }
            else
            {
                const int copper = bounty % 100;
                bounty = (bounty - copper) / 100;
                const int silver = bounty % 100;
                const int gold = (bounty - silver) / 100;
                result[0] = copper;
                result[1] = silver;
                result[2] = gold;
            }

            // Payment notification
            if (KillType == "Loot")
            {
                ss << "|cffFF0000[|cffff6060PVP|cffFF0000] Tu saqueas|cFFFFD700 ";
                sv << "|cffFF0000[|cffff6060PVP|cffFF0000]|cffFFFFFF " << player->GetName() << "|cffFF0000 reviso tu cadáver y toma |cFFFFD700 ";
            }
            else if (KillType == "PVP")
            {
                ss << "|cffFF0000[|cffff6060PVP|cffFF0000]|cffFFFFFF " << player->GetName() << "|cffFF0000 ha matado|cffFFFFFF "
                    << victim->GetName() << "|cffFF0000 ganando una recompensa de|cFFFFD700 ";
            }
            else
            {
                ss << "Recibes una recompensa de ";
            }

            // Figure out the money (todo: find a better way to handle the different strings)
            if (result[2] > 0)
            {
                ss << result[2] << " gold";
                sv << result[2] << " gold";
            }
            if (result[1] > 0)
            {
                if (result[2] > 0)
                {
                    ss << " " << result[1] << " silver";
                    sv << " " << result[1] << " silver";
                }
                else
                {
                    ss << result[1] << " silver";
                    sv << result[1] << " silver";

                }
            }
            if (result[0] > 0)
            {
                if (result[1] > 0)
                {
                    ss << " " << result[0] << " copper";
                    sv << " " << result[0] << " copper";
                }
                else
                {
                    ss << result[0] << " copper";
                    sv << result[0] << " copper";
                }
            }

            // Type of kill
            if (KillType == "Loot")
            {
                ss << "|cffFF0000 del cuerpo.";
                sv << "|cffFF0000.";
            }
            else if (KillType == "PVP")
            {
                ss << "|cffFF0000.";
                sv << "|cffFF0000.";
            }
            else
            {
                ss << " por la muerte.";
            }

            // If it's a boss kill..
            if (KillType == "WorldBoss")
            {
                std::ostringstream ss;

                // Is the player in a group? 
                if (player->GetGroup())
                {
                    // Only the party leader should announce the boss kill.
                    if (player->GetGroup()->GetLeaderName() == player->GetName())
                    {
                        // Chat icons don't show for all clients despite them showing when entered manually. Disabled until fix is found.
                        // Ex: {rt8} = {skull}
                        ss << "|cffFF0000[|cffff6060 BOSS KILL |cffFF0000]|cffFFFFFF " << player->GetName()
                            << "|cffFFFFFF's grupo triunf\xA2; victoriosamente sobre |cffFF0000[|cffff6060 "
                            << killed->GetName() << " |cffFF0000]|cffFFFFFF !";

                        sWorld->SendServerMessage(SERVER_MSG_STRING, ss.str().c_str());
                    }
                }
                else
                {
                    // Solo Kill
                    ss << "|cffFF0000[|cffff6060 BOSS KILL |cffFF0000]|cffFFFFFF " << player->GetName()
                        << "|cffFFFFFF triunf\xA2; victoriosamente sobre |cffFF0000[|cffff6060 "
                        << killed->GetName() << " |cffFF0000]|cffFFFFFF !";

                    sWorld->SendServerMessage(SERVER_MSG_STRING, ss.str().c_str());
                }
            }
            else if (KillType == "Loot")
            {
                // Inform the player of the corpse loot
                ChatHandler(player->GetSession()).SendSysMessage(ss.str().c_str());

                // Inform the victim of the corpse loot
                ChatHandler(victim->GetSession()).SendSysMessage(sv.str().c_str());
            }
            else if (KillType == "PVP")
            {
                // Inform the world of the kill
                sWorld->SendServerMessage(SERVER_MSG_STRING, ss.str().c_str());
            }
            else
            {
                if (KillType == "DungeonBoss")
                {
                    // Is the player in a group? 
                    if (player->GetGroup())
                    {
                        // Inform the player of the Dungeon Boss kill
                        std::ostringstream sb;
                        sb << "|cffFF8000 Tu equipo ha derrotado |cffFF0000" << killed->GetName() << "|cffFF8000 !";
                        ChatHandler(player->GetSession()).SendSysMessage(sb.str().c_str());
                    }
                    else
                    {
                        // Solo Kill
                        std::ostringstream sb;
                        sb << "|cffFF8000 Has derrotado |cffFF0000" << killed->GetName() << "|cffFF8000 !";
                        ChatHandler(player->GetSession()).SendSysMessage(sb.str().c_str());
                    }
                }

                // Inform the player of the bounty
                ChatHandler(player->GetSession()).SendSysMessage(ss.str().c_str());
            }
        }
    }
};
// Add all scripts in one
void AddMFKConfigScripts()
{
    new MFKConfig();
    new MFKAnnounce();
    new MoneyForKills();

}

