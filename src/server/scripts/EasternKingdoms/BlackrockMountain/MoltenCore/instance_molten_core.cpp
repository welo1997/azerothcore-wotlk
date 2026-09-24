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

#include "GameObjectScript.h"
#include "InstanceMapScript.h"
#include "InstanceScript.h"
#include "ObjectMgr.h"
#include "TemporarySummon.h"
#include "molten_core.h"

MinionData const minionData[] =
{
    { NPC_FIRESWORN,                DATA_GARR },
    { NPC_FLAMEWALKER,              DATA_GEHENNAS },
    { NPC_FLAMEWALKER_PROTECTOR,    DATA_LUCIFRON },
    { NPC_FLAMEWALKER_PRIEST,       DATA_SULFURON },
    { NPC_FLAMEWALKER_HEALER,       DATA_MAJORDOMO_EXECUTUS },
    { NPC_FLAMEWALKER_ELITE,        DATA_MAJORDOMO_EXECUTUS },
    { 0, 0 } // END
};

struct MCBossObject
{
    uint32 bossId;
    uint32 runeId;
    uint32 circleId;
};

constexpr uint8 MAX_MC_LINKED_BOSS_OBJ = 7;
MCBossObject const linkedBossObjData[MAX_MC_LINKED_BOSS_OBJ]=
{
    { DATA_MAGMADAR,    GO_RUNE_KRESS,      GO_CIRCLE_MAGMADAR  },
    { DATA_GEHENNAS,    GO_RUNE_MOHN,       GO_CIRCLE_GEHENNAS  },
    { DATA_GARR,        GO_RUNE_BLAZ,       GO_CIRCLE_GARR      },
    { DATA_SHAZZRAH,    GO_RUNE_MAZJ,       GO_CIRCLE_SHAZZRAH  },
    { DATA_GEDDON,      GO_RUNE_ZETH,       GO_CIRCLE_GEDDON    },
    { DATA_GOLEMAGG,    GO_RUNE_THERI,      GO_CIRCLE_GOLEMAGG  },
    { DATA_SULFURON,    GO_RUNE_KORO,       GO_CIRCLE_SULFURON  },
};

constexpr uint8 SAY_SPAWN = 1;

struct instance_molten_core : public InstanceScript
{
    instance_molten_core(Map* map) : InstanceScript(map)
    {
        SetHeaders(DataHeader);
        SetBossNumber(MAX_ENCOUNTER);
        LoadMinionData(minionData);
    }

    // 1.12 (cmangos/mangos-classic @ 8ec338a1704, molten_core.cpp
    // DoSpawnMajordomoIfCan()): re-entry only re-triggers the summon check,
    // it never bypasses the 7-rune requirement -- CheckMajordomoExecutus()
    // here covers the boss-DONE half, _dousedRuneMask covers the douse half.
    // The respawn branch (Majordomo already DONE, e.g. after a Ragnaros wipe)
    // is uiSummonPos==1 there and SummonMajordomoExecutus()'s else branch
    // here -- neither depends on the rune mask, both fire on prereqs alone.
    void OnPlayerEnter(Player* /*player*/) override
    {
        if (!CheckMajordomoExecutus())
            return;

        if (GetBossState(DATA_MAJORDOMO_EXECUTUS) == DONE ||
            _dousedRuneMask == ((1 << MAX_MC_LINKED_BOSS_OBJ) - 1))
            SummonMajordomoExecutus();
    }

    void OnCreatureCreate(Creature* creature) override
    {
        switch (creature->GetEntry())
        {
            case NPC_GOLEMAGG_THE_INCINERATOR:
            {
                _golemaggGUID = creature->GetGUID();
                break;
            }
            case NPC_CORE_RAGER:
            {
                _golemaggMinionsGUIDS.insert(creature->GetGUID());
                break;
            }
            case NPC_MAJORDOMO_EXECUTUS:
            {
                _majordomoExecutusGUID = creature->GetGUID();
                break;
            }
            case NPC_GARR:
            {
                _garrGUID = creature->GetGUID();
                break;
            }
            case NPC_RAGNAROS:
            {
                _ragnarosGUID = creature->GetGUID();
                break;
            }
            case NPC_FIRESWORN:
            case NPC_FLAMEWALKER:
            case NPC_FLAMEWALKER_PROTECTOR:
            case NPC_FLAMEWALKER_PRIEST:
            case NPC_FLAMEWALKER_HEALER:
            case NPC_FLAMEWALKER_ELITE:
            {
                AddMinion(creature);
                break;
            }
        }
    }

    void OnCreatureRemove(Creature* creature) override
    {
        switch (creature->GetEntry())
        {
            case NPC_FIRESWORN:
            {
                RemoveMinion(creature);
                break;
            }
            case NPC_FLAMEWALKER:
            case NPC_FLAMEWALKER_PROTECTOR:
            case NPC_FLAMEWALKER_PRIEST:
            case NPC_FLAMEWALKER_HEALER:
            case NPC_FLAMEWALKER_ELITE:
            {
                RemoveMinion(creature);
                break;
            }
        }
    }

    void OnGameObjectCreate(GameObject* go) override
    {
        switch (go->GetEntry())
        {
            case GO_CACHE_OF_THE_FIRELORD:
            {
                _cacheOfTheFirelordGUID = go->GetGUID();
                break;
            }
            case GO_CIRCLE_GEDDON:
            case GO_CIRCLE_GARR:
            case GO_CIRCLE_GEHENNAS:
            case GO_CIRCLE_GOLEMAGG:
            case GO_CIRCLE_MAGMADAR:
            case GO_CIRCLE_SHAZZRAH:
            case GO_CIRCLE_SULFURON:
            {
                for (uint8 i = 0; i < MAX_MC_LINKED_BOSS_OBJ; ++i)
                {
                    if (linkedBossObjData[i].circleId != go->GetEntry())
                        continue;

                    if (GetBossState(linkedBossObjData[i].bossId) == DONE)
                        go->DespawnOrUnsummon(0ms, Seconds(WEEK));
                    else
                        _circlesGUIDs[linkedBossObjData[i].bossId] = go->GetGUID();
                }

                break;
            }
            case GO_RUNE_KRESS:
            case GO_RUNE_MOHN:
            case GO_RUNE_BLAZ:
            case GO_RUNE_MAZJ:
            case GO_RUNE_ZETH:
            case GO_RUNE_THERI:
            case GO_RUNE_KORO:
            {
                for (uint8 i = 0; i < MAX_MC_LINKED_BOSS_OBJ; ++i)
                {
                    if (linkedBossObjData[i].runeId != go->GetEntry())
                        continue;

                    // Restore this rune's open visual on instance load: it
                    // reflects a manual douse (_dousedRuneMask, persisted via
                    // ReadSaveDataMore), never the boss's own state -- a dead
                    // prereq boss with an undoused rune must stay closed
                    // (cmangos molten_core.cpp OnObjectCreate: `GetData(...)
                    // == SPECIAL`, the rune's own persisted flag, not the
                    // boss's).
                    if (_dousedRuneMask & (1 << i))
                        go->UseDoorOrButton(WEEK * IN_MILLISECONDS);
                    else
                        _runesGUIDs[linkedBossObjData[i].bossId] = go->GetGUID();
                }
                break;
            }
            case GO_LAVA_STEAM:
            {
                _lavaSteamGUID = go->GetGUID();
                break;
            }
            case GO_LAVA_SPLASH:
            {
                _lavaSplashGUID = go->GetGUID();
                break;
            }
            case GO_LAVA_BURST:
            {
                if (Creature* ragnaros = instance->GetCreature(_ragnarosGUID))
                    ragnaros->AI()->SetGUID(go->GetGUID(), GO_LAVA_BURST);
                break;
            }
        }
    }

    ObjectGuid GetGuidData(uint32 type) const override
    {
        switch (type)
        {
            case DATA_GOLEMAGG:
                return _golemaggGUID;
            case DATA_MAJORDOMO_EXECUTUS:
                return _majordomoExecutusGUID;
            case DATA_GARR:
                return _garrGUID;
            case DATA_LAVA_STEAM:
                return _lavaSteamGUID;
            case DATA_LAVA_SPLASH:
                return _lavaSplashGUID;
            case DATA_RAGNAROS:
                return _ragnarosGUID;
        }

        return ObjectGuid::Empty;
    }

    bool SetBossState(uint32 bossId, EncounterState state) override
    {
        if (!InstanceScript::SetBossState(bossId, state))
            return false;

        if (bossId == DATA_MAJORDOMO_EXECUTUS && state == DONE)
        {
            if (GameObject* cache = instance->GetGameObject(_cacheOfTheFirelordGUID))
            {
                cache->SetRespawnTime(7 * DAY);
                cache->SetLootRecipient(instance);
            }
        }
        else if (bossId == DATA_GOLEMAGG)
        {
            switch (state)
            {
                case NOT_STARTED:
                case FAIL:
                {
                    if (!_golemaggMinionsGUIDS.empty())
                    {
                        for (ObjectGuid const& minionGuid : _golemaggMinionsGUIDS)
                        {
                            Creature* minion = instance->GetCreature(minionGuid);
                            if (minion && minion->isDead())
                                minion->Respawn();
                        }
                    }
                    break;
                }
                case IN_PROGRESS:
                {
                    if (!_golemaggMinionsGUIDS.empty())
                    {
                        for (ObjectGuid const& minionGuid : _golemaggMinionsGUIDS)
                        {
                            if (Creature* minion = instance->GetCreature(minionGuid))
                                minion->AI()->DoZoneInCombat(nullptr, 150.0f);
                        }
                    }
                    break;
                }
                case DONE:
                {
                    if (!_golemaggMinionsGUIDS.empty())
                    {
                        for (ObjectGuid const& minionGuid : _golemaggMinionsGUIDS)
                        {
                            if (Creature* minion = instance->GetCreature(minionGuid))
                                minion->CastSpell(minion, SPELL_CORE_RAGER_QUIET_SUICIDE, true);
                        }
                        _golemaggMinionsGUIDS.clear();
                    }
                    break;
                }
                default:
                    break;
            }
        }
        // Perform needed checks for Majordomu
        if (bossId < DATA_MAJORDOMO_EXECUTUS && state == DONE)
        {
            if (GameObject* circle = instance->GetGameObject(_circlesGUIDs[bossId]))
            {
                circle->DespawnOrUnsummon(0ms, Seconds(WEEK));
                _circlesGUIDs[bossId].Clear();
            }

            // 1.12: the rune lights (becomes usable) once its boss is dead, but
            // dousing it is a manual player action (GO_FLAG_LOCKED clear only) --
            // it does not open itself, and Majordomo does not spawn here. See
            // UseRune() below, which is the only path that opens a rune and the
            // only path that can spawn Majordomo from the 7-douse count.
        }

        return true;
    }

    // Called from go_molten_core_rune::OnGossipHello (rune GOs carry that
    // ScriptName, sql/world/366_encounters_core_2.sql). Returns true if the
    // rune was doused (opened) by this use, false if it was rejected because
    // its boss is not yet dead (1.12 mechanic: dousing an undoused rune whose
    // boss lives does not progress the encounter).
    bool UseRune(GameObject* rune)
    {
        for (uint8 i = 0; i < MAX_MC_LINKED_BOSS_OBJ; ++i)
        {
            if (linkedBossObjData[i].runeId != rune->GetEntry())
                continue;

            if (GetBossState(linkedBossObjData[i].bossId) != DONE)
                return false;

            if (!(_dousedRuneMask & (1 << i)))
            {
                _dousedRuneMask |= (1 << i);
                rune->UseDoorOrButton(WEEK * IN_MILLISECONDS);

                if (_dousedRuneMask == ((1 << MAX_MC_LINKED_BOSS_OBJ) - 1) && CheckMajordomoExecutus())
                    SummonMajordomoExecutus();
            }

            return true;
        }

        return false;
    }

    void DoAction(int32 action) override
    {
        if (action == ACTION_RESET_GOLEMAGG_ENCOUNTER)
        {
            if (Creature* golemagg = instance->GetCreature(_golemaggGUID))
                golemagg->AI()->EnterEvadeMode();

            if (!_golemaggMinionsGUIDS.empty())
            {
                for (ObjectGuid const& minionGuid : _golemaggMinionsGUIDS)
                {
                    if (Creature* minion = instance->GetCreature(minionGuid))
                        minion->AI()->EnterEvadeMode();
                }
            }
        }
    }

    void SummonMajordomoExecutus()
    {
        if (instance->GetCreature(_majordomoExecutusGUID))
            return;

        if (GetBossState(DATA_MAJORDOMO_EXECUTUS) != DONE)
        {
            if (Creature* creature = instance->SummonCreature(NPC_MAJORDOMO_EXECUTUS, MajordomoSummonPos))
                creature->AI()->Talk(SAY_SPAWN);
        }
        else
        {
            instance->SummonCreature(NPC_MAJORDOMO_EXECUTUS, MajordomoRagnaros);
        }
    }

    // uint8 read via istream extraction is treated as a single char, not a
    // parsed integer (same trap other instance scripts avoid by using
    // uint32 fields, e.g. instance_shadowfang_keep.cpp's _encounters[]) --
    // _dousedRuneMask is declared uint32 below for exactly that reason, even
    // though only its low 7 bits are ever set. A save written before this
    // unit has no trailing field: the extraction fails, the stream sets
    // failbit, and _dousedRuneMask keeps its member-initializer value (0) --
    // backward compatible with no explicit fallback needed.
    void ReadSaveDataMore(std::istringstream& data) override
    {
        data >> _dousedRuneMask;
    }

    void WriteSaveDataMore(std::ostringstream& data) override
    {
        data << _dousedRuneMask;
    }

    bool CheckMajordomoExecutus() const
    {
        if (GetBossState(DATA_RAGNAROS) == DONE)
            return false;

        for (uint8 i = 0; i < DATA_MAJORDOMO_EXECUTUS; ++i)
        {
            if (i == DATA_LUCIFRON)
                continue;

            if (GetBossState(i) != DONE)
                return false;
        }

        // Prevent spawning if Ragnaros is present
        if (instance->GetCreature(_ragnarosGUID))
            return false;

        return true;
    }

private:
    std::unordered_map<uint32/*bossid*/, ObjectGuid/*circleGUID*/> _circlesGUIDs;
    std::unordered_map<uint32/*bossid*/, ObjectGuid/*runeGUID*/> _runesGUIDs;

    // Golemagg encounter related
    ObjectGuid _golemaggGUID;
    GuidSet _golemaggMinionsGUIDS;

    // Ragnaros encounter related
    ObjectGuid _ragnarosGUID;
    ObjectGuid _lavaSteamGUID;
    ObjectGuid _lavaSplashGUID;

    ObjectGuid _majordomoExecutusGUID;
    ObjectGuid _cacheOfTheFirelordGUID;
    ObjectGuid _garrGUID;
    ObjectGuid _magmadarGUID;

    // Bit i set once linkedBossObjData[i]'s rune has been manually doused.
    // Persisted via ReadSaveDataMore/WriteSaveDataMore below.
    uint32 _dousedRuneMask = 0;
};

class go_molten_core_rune : public GameObjectScript
{
public:
    go_molten_core_rune() : GameObjectScript("go_molten_core_rune") { }

    bool OnGossipHello(Player* /*player*/, GameObject* go) override
    {
        if (InstanceScript* instance = go->GetInstanceScript())
            if (auto* mc = dynamic_cast<instance_molten_core*>(instance))
                mc->UseRune(go);

        // Always consume the click: opening (or not) is handled inside
        // UseRune() so a rejected douse produces no visual/state change.
        return true;
    }
};

void AddSC_instance_molten_core()
{
    RegisterInstanceScript(instance_molten_core, MAP_MOLTEN_CORE);
    new go_molten_core_rune();
}
