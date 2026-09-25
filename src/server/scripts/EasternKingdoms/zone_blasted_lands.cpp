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
#include "PlayerScript.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
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
//
// Vanilla-plus, 2026-09-23 (worldboss-ai unit): 12397 roams via 188_'s ScriptName wiring
// (Reset() below) but the AC stock template carries no spell kit at all -- owner report,
// live: "does he have spells? he isnt using any." Spell ids cross-checked against
// AzerothCore's own Outland/boss_doomlord_kazzak.cpp (the TBC "Doom Lord Kazzak" upgrade
// of this same NPC, which reuses 21063 Twisted Reflection verbatim from this era) plus
// Wowhead Classic and Warcraft Wiki, each independently: 21341 Shadow Bolt Volley, 16044
// Cleave, 15588 Thunderclap, 21056 Mark of Kazzak all resolved and cross-referenced --
// none invented. The "gains health when Mark's target dies" mechanic is this brief's own
// spec (not the TBC version's mana-depletion AOE explode, a different, harder mechanic
// this unit did not implement); heal amount (5% of Kazzak's max health) is this unit's
// own tuning choice, not a sourced vanilla number -- flagged for the owner to retune.

enum KazzakSpells
{
    SPELL_KAZZAK_SHADOW_BOLT_VOLLEY = 21341,
    SPELL_KAZZAK_CLEAVE             = 16044,
    SPELL_KAZZAK_THUNDERCLAP        = 15588,
    SPELL_KAZZAK_MARK_OF_KAZZAK     = 21056
};

class boss_kazzak : public CreatureScript
{
public:
    boss_kazzak() : CreatureScript("boss_kazzak") { }

    struct boss_kazzakAI : public ScriptedAI
    {
        boss_kazzakAI(Creature* creature) : ScriptedAI(creature) { }

        void Reset() override
        {
            scheduler.CancelAll();
            // Reset() also runs on every evade, where he stands wherever the fight ended; a new
            // generator would re-centre the roam (and the leash) there, so he drifted across the
            // zone. Start the roam once, at his spawn, and keep it.
            if (me->GetMotionMaster()->GetMotionSlotType(MOTION_SLOT_IDLE) != RANDOM_MOTION_TYPE)
                me->GetMotionMaster()->MoveRandom(40.0f);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            scheduler
                .Schedule(3s, 8s, [this](TaskContext context)
                {
                    DoCastVictim(SPELL_KAZZAK_SHADOW_BOLT_VOLLEY);
                    context.Repeat(8s, 14s);
                })
                .Schedule(5s, 10s, [this](TaskContext context)
                {
                    DoCastVictim(SPELL_KAZZAK_CLEAVE);
                    context.Repeat(8s, 12s);
                })
                .Schedule(5s, 10s, [this](TaskContext context)
                {
                    DoCastVictim(SPELL_KAZZAK_THUNDERCLAP);
                    context.Repeat(10s, 15s);
                })
                .Schedule(15s, 25s, [this](TaskContext context)
                {
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, PowerUsersSelector(me, POWER_MANA, 100.0f, true)))
                    {
                        DoCast(target, SPELL_KAZZAK_MARK_OF_KAZZAK);
                    }
                    context.Repeat(20s, 30s);
                });
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
            {
                return;
            }

            scheduler.Update(diff);
            if (me->HasUnitState(UNIT_STATE_CASTING))
            {
                return;
            }

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new boss_kazzakAI(creature);
    }
};

// worldboss-ai (2026-09-23) shipped a heal-on-death hook tied to the Mark of
// Kazzak aura itself (this class used to live here as `spell_mark_of_kazzak`,
// 5% of max HP) as its own untuned guess -- it was never bound to spell 21056
// by any `spell_script_names` row (Realm A boot log: "Script named
// 'spell_mark_of_kazzak' is not assigned in the database"), so it never fired.
// Two independent sources agree the real 1.12 mechanic is a *separate* passive
// unrelated to the Mark -- Capture Soul, a flat 70,000 HP heal whenever any
// nearby player dies -- so the stale, never-bound class is replaced outright
// rather than bound as written (encounters-remaining-2026-09-23.md, "Mark of
// Kazzak's heal amount"). Owner decision on the exact number is still
// formally open (recorded there); this ships the driver's stated default.
enum KazzakCaptureSoul
{
    NPC_KAZZAK                  = 12397,
    CAPTURE_SOUL_HEAL           = 70000,
    CAPTURE_SOUL_RADIUS         = 100 // yards -- "large radius" per the sources, no exact value found
};

class player_kazzak_capture_soul : public PlayerScript
{
public:
    player_kazzak_capture_soul() : PlayerScript("player_kazzak_capture_soul") { }

    void OnPlayerJustDied(Player* player) override
    {
        std::list<Creature*> kazzaks;
        GetCreatureListWithEntryInGrid(kazzaks, player, NPC_KAZZAK, CAPTURE_SOUL_RADIUS);

        for (Creature* kazzak : kazzaks)
            if (kazzak->IsAlive())
                kazzak->SetHealth(std::min(kazzak->GetMaxHealth(), kazzak->GetHealth() + CAPTURE_SOUL_HEAL));
    }
};

void AddSC_blasted_lands()
{
    RegisterSpellScript(spell_razelikh_teleport_group);
    new boss_kazzak();
    new player_kazzak_capture_soul();
}
