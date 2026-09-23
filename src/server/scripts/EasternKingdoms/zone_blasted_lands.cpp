/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
Blasted_Lands
Quest support: 3628. Teleporter to Rise of the Defiler.
*/

#include "Group.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"

/*#####
# spell_razelikh_teleport_group
#####*/

enum DeathlyUsher
{
    SPELL_TELEPORT_SINGLE               = 12885,
    SPELL_TELEPORT_SINGLE_IN_GROUP      = 13142,
    SPELL_TELEPORT_GROUP                = 27686
};

class spell_razelikh_teleport_group : public SpellScript
{
    PrepareSpellScript(spell_razelikh_teleport_group);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_TELEPORT_SINGLE, SPELL_TELEPORT_SINGLE_IN_GROUP });
    }

    void HandleScriptEffect(SpellEffIndex /* effIndex */)
    {
        if (Player* player = GetHitPlayer())
        {
            if (Group* group = player->GetGroup())
            {
                for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
                    if (Player* member = itr->GetSource())
                        if (member->IsWithinDistInMap(player, 20.0f) && !member->isDead())
                            member->CastSpell(member, SPELL_TELEPORT_SINGLE_IN_GROUP, true);
            }
            else
                player->CastSpell(player, SPELL_TELEPORT_SINGLE, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_razelikh_teleport_group::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

/*#####
# boss_kazzak
#####*/

// The 1.12 dump's row for Kazzak carries wander_distance/spawndist = 0 (see
// sql/world/184_kazzak_spawn.sql), so his roam cannot be expressed through the
// DB's RandomMovementGenerator/WaypointMovementGenerator the way an ordinary
// spawn's wander_distance would -- a nonzero radius here would be inventing
// data the 1.12 source doesn't have. This gives him the same "roam a radius
// while idle" behavior in code instead, matching a MovementType/wander_distance
// pair no vanilla-1.12 dump value can express.
class boss_kazzak : public CreatureScript
{
public:
    boss_kazzak() : CreatureScript("boss_kazzak") { }

    struct boss_kazzakAI : public ScriptedAI
    {
        boss_kazzakAI(Creature* creature) : ScriptedAI(creature) { }

        void Reset() override
        {
            me->GetMotionMaster()->MoveRandom(40.0f);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new boss_kazzakAI(creature);
    }
};

void AddSC_blasted_lands()
{
    RegisterSpellScript(spell_razelikh_teleport_group);
    new boss_kazzak();
}
