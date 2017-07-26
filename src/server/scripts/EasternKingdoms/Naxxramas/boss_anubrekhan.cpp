/*
 * Copyright (C) 2011-2017 Project SkyFire <http://www.projectskyfire.org/>
 * Copyright (C) 2008-2017 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2010-2017 Oregon <http://www.oregoncore.com/>
 * Copyright (C) 2005-2017 MaNGOS <https://www.getmangos.eu/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptPCH.h"
#include "naxxramas.h"

#define SAY_GREET           RAND(-1533000,-1533004,-1533005,-1533006,-1533007)
#define SAY_AGGRO           RAND(-1533001,-1533002,-1533003)
#define SAY_SLAY            -1533008

#define MOB_CRYPT_GUARD     16573

const Position GuardSummonPos = {3333.72f, -3476.30f, 287.1f, 6.2801f};

enum Events
{
    EVENT_NONE,
    EVENT_IMPALE,
    EVENT_LOCUST,
    EVENT_SPAWN_GUARDIAN_NORMAL,
    EVENT_BERSERK,
};

enum Spells
{
    SPELL_IMPALE_10                 = 28783,
    SPELL_IMPALE_25                 = 56090,
    SPELL_LOCUST_SWARM_10           = 28785,
    SPELL_LOCUST_SWARM_25           = 54021,
    SPELL_SUMMON_CORPSE_SCARABS_PLR = 29105,    // This spawns 5 corpse scarabs on top of player
    SPELL_SUMMON_CORPSE_SCARABS_MOB = 28864,   // This spawns 10 corpse scarabs on top of dead guards
    SPELL_BERSERK                   = 27680,
};

enum
{
    ACHIEV_TIMED_START_EVENT                      = 9891,
};

class boss_anubrekhan : public CreatureScript
{
public:
    boss_anubrekhan() : CreatureScript("boss_anubrekhan") { }

    CreatureAI* GetAI(Creature* pCreature) const
    {
        return new boss_anubrekhanAI (pCreature);
    }

    struct boss_anubrekhanAI : public BossAI
    {
        boss_anubrekhanAI(Creature *c) : BossAI(c, BOSS_ANUBREKHAN) {}

        bool hasTaunted;

        void Reset()
        {
            _Reset();
            SetImmuneToDeathGrip();

            hasTaunted = false;

            //if (getDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL)
            //{
                Position pos;

                // respawn guard using home position,
                // otherwise, after a wipe, they respawn where boss was at wipe moment.
                pos = me->GetHomePosition();
                pos.m_positionY -= 10.0f;
                me->SummonCreature(MOB_CRYPT_GUARD, pos, TEMPSUMMON_CORPSE_DESPAWN);

            if (getDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL)
            {
                pos = me->GetHomePosition();
                pos.m_positionY += 10.0f;
                me->SummonCreature(MOB_CRYPT_GUARD, pos, TEMPSUMMON_CORPSE_DESPAWN);
            }
        }

        void KilledUnit(Unit* victim)
        {
            //Force the player to spawn corpse scarabs via spell, TODO: Check percent chance for scarabs, 20% at the moment
            if (!(rand()%5))
                if (victim->GetTypeId() == TYPEID_PLAYER)
                    victim->CastSpell(victim, SPELL_SUMMON_CORPSE_SCARABS_PLR, true, NULL, NULL, me->GetGUID());

            DoScriptText(SAY_SLAY, me);
        }

        void JustDied(Unit *)
        {
            _JustDied();

            // start achievement timer (kill Maexna within 20 min)
            if (instance)
                instance->DoStartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_EVENT, ACHIEV_TIMED_START_EVENT);
        }

        void EnterCombat(Unit * /*who*/)
        {
            _EnterCombat();
            DoScriptText(SAY_AGGRO, me);
            events.ScheduleEvent(EVENT_IMPALE, 10000 + rand()%10000);
            events.ScheduleEvent(EVENT_LOCUST, urand(80000,120000));
            events.ScheduleEvent(EVENT_BERSERK, 600000);

            //if (getDifficulty() == RAID_DIFFICULTY_10MAN_NORMAL)
                events.ScheduleEvent(EVENT_SPAWN_GUARDIAN_NORMAL, urand(15000,20000));
        }

        void MoveInLineOfSight(Unit *who)
        {
            if (!hasTaunted && me->IsWithinDistInMap(who, 60.0f) && who->GetTypeId() == TYPEID_PLAYER)
            {
                DoScriptText(SAY_GREET, me);
                hasTaunted = true;
            }
            ScriptedAI::MoveInLineOfSight(who);
        }

        void SummonedCreatureDespawn(Creature *summon)
        {
            BossAI::SummonedCreatureDespawn(summon);

            // check if it is an actual killed guard
            if (!me->isAlive() || summon->isAlive() || summon->GetEntry() != MOB_CRYPT_GUARD)
                return;

            summon->CastSpell(summon, SPELL_SUMMON_CORPSE_SCARABS_MOB, true, NULL, NULL, me->GetGUID());
        }

        void UpdateAI(const uint32 diff)
        {
            if (!UpdateVictim() || !CheckInRoom())
                return;

            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch(eventId)
                {
                    case EVENT_IMPALE:
                        if(!me->IsNonMeleeSpellCasted(false))
                        {
                        //Cast Impale on a random target
                        //Do NOT cast it when we are afflicted by locust swarm
                        if (!me->HasAura(RAID_MODE(SPELL_LOCUST_SWARM_10,SPELL_LOCUST_SWARM_25)))
                            if (Unit *pTarget = SelectUnit(SELECT_TARGET_RANDOM, 0))
                                DoCast(pTarget, RAID_MODE(SPELL_IMPALE_10,SPELL_IMPALE_25));
                        events.ScheduleEvent(EVENT_IMPALE, urand(10000,20000));
                        }
                        break;
                    case EVENT_LOCUST:
                        if(!me->IsNonMeleeSpellCasted(false))
                        {
                        // TODO : Add Text
                        DoCast(me, RAID_MODE(SPELL_LOCUST_SWARM_10,SPELL_LOCUST_SWARM_25));
                        DoSummon(MOB_CRYPT_GUARD, GuardSummonPos, 0, TEMPSUMMON_CORPSE_DESPAWN);
                            events.ScheduleEvent(EVENT_LOCUST, urand(85000,95000));
                        }
                        break;
                    case EVENT_SPAWN_GUARDIAN_NORMAL:
                        // TODO : Add Text
                        DoSummon(MOB_CRYPT_GUARD, GuardSummonPos, 0, TEMPSUMMON_CORPSE_DESPAWN);
                        break;
                    case EVENT_BERSERK:
                        if (getDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL)
                        DoCast(me, SPELL_BERSERK, true);
                        events.ScheduleEvent(EVENT_BERSERK, 600000);
                        break;
                }
            }

            DoMeleeAttackIfReady();
        }
    };
};

void AddSC_boss_anubrekhan()
{
    new boss_anubrekhan();
}
