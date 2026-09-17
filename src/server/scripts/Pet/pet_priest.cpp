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
 * Ordered alphabetically using scriptname.
 * Scriptnames of files in this file should be prefixed with "npc_pet_pri_".
 */

#include "CreatureScript.h"
#include "Log.h"
#include "PetAI.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "TotemAI.h"

enum PriestSpells
{
    SPELL_PRIEST_LIGHTWELL_CHARGES          = 59907,

    // T0.5 Priest Healing (900104) 8pc set bonus, Lightwell half -- core-C++
    // #13, 2026-09-16 (docs/design/analysis/item-port-disposition-2026-09-16.md
    // B.2 #13). 90600 is the already-shipped passive granted by the 8pc slot
    // (sql/world/77_t05_wave_b.sql), reused here as the per-player gate --
    // not a new spell.
    SPELL_PRIEST_T05_LIGHTWELL_8PC          = 90600,
};

// T0.5 Lightwell bonus charge count -- see InitializeAI below for why this
// cannot be a ModCharges() call.
uint8 const T05_LIGHTWELL_BONUS_CHARGES = 4;

struct npc_pet_pri_lightwell : public TotemAI
{
    npc_pet_pri_lightwell(Creature* creature) : TotemAI(creature) { }

    void InitializeAI() override
    {
        Unit* owner = nullptr;
        if (TempSummon* tempSummon = me->ToTempSummon())
        {
            owner = tempSummon->GetSummonerUnit();
            if (owner)
            {
                uint32 hp = uint32(owner->GetMaxHealth() * 0.3f);
                me->SetMaxHealth(hp);
                me->SetHealth(hp);
                me->SetLevel(owner->GetLevel());
            }
        }

        me->CastSpell(me, SPELL_PRIEST_LIGHTWELL_CHARGES, false); // Spell for Lightwell Charges

        // DEBUG instrumentation (builder-lightwell-charges, temporary):
        // does the non-triggered self-cast above actually land the charges aura?
        if (Aura* debugChargesAura = me->GetAura(SPELL_PRIEST_LIGHTWELL_CHARGES))
            LOG_INFO("scripts", "lightwell-debug: self-cast landed, charges={}", debugChargesAura->GetCharges());
        else
            LOG_INFO("scripts", "lightwell-debug: self-cast of 59907 did NOT land (GetAura returned null)");

        // T0.5 8pc: +4 usable charges when the summoner owns the set bonus.
        // Aura::ModCharges() only ever clamps a charge count UP to
        // CalcMaxCharges() (SpellAuras.cpp), and the charges aura just cast
        // above is already created at that same max (Aura::Create sets
        // m_procCharges = CalcMaxCharges() on construction) -- so
        // ModCharges(+4) here would be a silent no-op, clamped straight back
        // down. SetCharges() bypasses that clamp, which is what "4 more
        // charges than stock" actually requires; the later per-cast
        // ModCharges(-1) in spell_pri_lightwell (spell_priest.cpp) only ever
        // decrements, so it is unaffected by the inflated starting count.
        if (owner && owner->HasAura(SPELL_PRIEST_T05_LIGHTWELL_8PC))
        {
            if (Aura* chargesAura = me->GetAura(SPELL_PRIEST_LIGHTWELL_CHARGES))
                chargesAura->SetCharges(uint8(chargesAura->GetCharges() + T05_LIGHTWELL_BONUS_CHARGES));
        }

        TotemAI::InitializeAI();
    }
};

void AddSC_priest_pet_scripts()
{
    RegisterCreatureAI(npc_pet_pri_lightwell);
}
