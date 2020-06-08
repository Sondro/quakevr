/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2020 Vittorio Romeo

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// world.c -- world query functions

#include "quakedef.hpp"
#include "util.hpp"
#include "quakeglm.hpp"
#include "vr.hpp"
#include "vr_cvars.hpp"

#include <cassert>

/*

entities never clip against themselves, or their owner

line of sight checks trace->crosscontent, but bullets don't

*/


struct moveclip_t
{
    qvec3 boxmins, boxmaxs; // enclose the test object along entire move
    qvec3 mins, maxs;       // size of the moving object
    qvec3 mins2, maxs2;     // size when clipping against mosnters
    qvec3 start, end;
    trace_t trace;
    int type;
    edict_t* passedict;
};


int SV_HullPointContents(hull_t* hull, int num, const qvec3& p);

/*
===============================================================================

HULL BOXES

===============================================================================
*/


static hull_t box_hull;
static mclipnode_t box_clipnodes[6]; // johnfitz -- was dclipnode_t
static mplane_t box_planes[6];

/*
===================
SV_InitBoxHull

Set up the planes and clipnodes so that the six floats of a bounding box
can just be stored out and get a proper hull_t structure.
===================
*/
void SV_InitBoxHull()
{
    int i;
    int side;

    box_hull.clipnodes = box_clipnodes;
    box_hull.planes = box_planes;
    box_hull.firstclipnode = 0;
    box_hull.lastclipnode = 5;

    for(i = 0; i < 6; i++)
    {
        box_clipnodes[i].planenum = i;

        side = i & 1;

        box_clipnodes[i].children[side] = CONTENTS_EMPTY;
        if(i != 5)
        {
            box_clipnodes[i].children[side ^ 1] = i + 1;
        }
        else
        {
            box_clipnodes[i].children[side ^ 1] = CONTENTS_SOLID;
        }

        box_planes[i].type = i >> 1;
        box_planes[i].normal[i >> 1] = 1;
    }
}


/*
===================
SV_HullForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
hull_t* SV_HullForBox(const qvec3& mins, const qvec3& maxs)
{
    box_planes[0].dist = maxs[0];
    box_planes[1].dist = mins[0];
    box_planes[2].dist = maxs[1];
    box_planes[3].dist = mins[1];
    box_planes[4].dist = maxs[2];
    box_planes[5].dist = mins[2];

    return &box_hull;
}



/*
================
SV_HullForEntity

Returns a hull that can be used for testing or clipping an object of mins/maxs
size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
hull_t* SV_HullForEntity(edict_t* ent, const qfvec3& mins,
    const qfvec3& maxs, qfvec3& offset)
{
    qfvec3 size;
    qfvec3 hullmins;
    qfvec3 hullmaxs;

    hull_t* hull;

    // decide which clipping hull to use, based on the size
    if(ent->v.solid == SOLID_BSP)
    {
        // explicit hulls in the BSP model
        if(ent->v.movetype != MOVETYPE_PUSH)
        {
            Host_Error("SOLID_BSP without MOVETYPE_PUSH (%s at %f %f %f)",
                PR_GetString(ent->v.classname), ent->v.origin[0],
                ent->v.origin[1], ent->v.origin[2]);
        }

        qmodel_t* const model = sv.models[(int)ent->v.modelindex];

        if(!model || model->type != mod_brush)
        {
            Host_Error("SOLID_BSP with a non bsp model (%s at %f %f %f)",
                PR_GetString(ent->v.classname), ent->v.origin[0],
                ent->v.origin[1], ent->v.origin[2]);
        }

        size = maxs - mins;

        if(size[0] < 3)
        {
            hull = &model->hulls[0];
        }
        else if(size[0] <= 32)
        {
            hull = &model->hulls[1];
        }
        else
        {
            hull = &model->hulls[2];
        }

        // calculate an offset value to center the origin
        offset = hull->clip_mins - mins;
        offset += ent->v.origin;
    }
    else
    {
        // create a temp hull from bounding box sizes

        hullmins = ent->v.mins - maxs;
        hullmaxs = ent->v.maxs - mins;
        hull = SV_HullForBox(hullmins, hullmaxs);

        offset = ent->v.origin;
    }


    return hull;
}

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/

typedef struct areanode_s
{
    int axis; // -1 = leaf node
    float dist;
    struct areanode_s* children[2];
    link_t trigger_edicts;
    link_t solid_edicts;
} areanode_t;

#define AREA_DEPTH 4
#define AREA_NODES 32

static areanode_t sv_areanodes[AREA_NODES];
static int sv_numareanodes;

/*
===============
SV_CreateAreaNode

===============
*/
areanode_t* SV_CreateAreaNode(
    int depth, const qvec3& mins, const qvec3& maxs)
{
    areanode_t* anode = &sv_areanodes[sv_numareanodes];
    sv_numareanodes++;

    ClearLink(&anode->trigger_edicts);
    ClearLink(&anode->solid_edicts);

    if(depth == AREA_DEPTH)
    {
        anode->axis = -1;
        anode->children[0] = anode->children[1] = nullptr;
        return anode;
    }

    qvec3 size = maxs - mins;
    if(size[0] > size[1])
    {
        anode->axis = 0;
    }
    else
    {
        anode->axis = 1;
    }

    anode->dist = 0.5f * (maxs[anode->axis] + mins[anode->axis]);

    const qvec3& mins1 = mins;
    qvec3 mins2 = mins;
    qvec3 maxs1 = maxs;
    const qvec3& maxs2 = maxs;

    maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

    anode->children[0] = SV_CreateAreaNode(depth + 1, mins2, maxs2);
    anode->children[1] = SV_CreateAreaNode(depth + 1, mins1, maxs1);

    return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld()
{
    SV_InitBoxHull();

    memset(sv_areanodes, 0, sizeof(sv_areanodes));
    sv_numareanodes = 0;
    SV_CreateAreaNode(0, sv.worldmodel->mins, sv.worldmodel->maxs);
}


/*
===============
SV_UnlinkEdict

===============
*/
void SV_UnlinkEdict(edict_t* ent)
{
    if(!ent->area.prev)
    {
        return; // not linked in anywhere
    }
    RemoveLink(&ent->area);
    ent->area.prev = ent->area.next = nullptr;
}


/*
====================
SV_AreaTriggerEdicts

Spike -- just builds a list of entities within the area, rather than walking
them and risking the list getting corrupt.
====================
*/
static void SV_AreaTriggerEdicts(edict_t* ent, areanode_t* node, edict_t** list,
    int* listcount, const int listspace)
{
    const auto loopEdicts = [&](link_t& edictList) {
        link_t* next;

        for(link_t* l = edictList.next; l != &edictList; l = next)
        {
            next = l->next;
            edict_t* target = EDICT_FROM_AREA(l);

            if(target == ent)
            {
                continue;
            }

            const bool canBeTouched = quake::util::canBeTouched(target);

            // TODO VR: (P2) consequences of this? Seems to fix handtouch on
            // ledges
            // if(!canBeTouched || !quake::util::entBoxIntersection(ent,
            // target))
            if(!canBeTouched)
            {
                continue;
            }

            if(*listcount == listspace)
            {
                // should never happen
                assert(false);
                return false;
            }

            list[*listcount] = target;
            (*listcount)++;
        }

        return true;
    };

    // touch linked edicts
    // TODO VR: (P2) hack - what is the consequence of adding this loop over
    // solid edicts? need testing
    const bool triggerFail = !loopEdicts(node->trigger_edicts);
    const bool solidFail = !loopEdicts(node->solid_edicts);
    if(triggerFail && solidFail)
    {
        // TODO VR: (P2) this prevents the other one from being ran, but the
        // return was originally there in quake's source code
        return;
    }

    // recurse down both sides
    if(node->axis == -1)
    {
        return;
    }

    if(ent->v.absmax[node->axis] > node->dist)
    {
        SV_AreaTriggerEdicts(
            ent, node->children[0], list, listcount, listspace);
    }

    if(ent->v.absmin[node->axis] < node->dist)
    {
        SV_AreaTriggerEdicts(
            ent, node->children[1], list, listcount, listspace);
    }
}

/*
====================
SV_TouchLinks

ericw -- copy the touching edicts to an array (on the hunk) so we can avoid
iteating the trigger_edicts linked list while calling PR_ExecuteProgram
which could potentially corrupt the list while it's being iterated.
Based on code from Spike.
====================
*/
void SV_TouchLinks(edict_t* ent)
{
    const int mark = Hunk_LowMark();
    edict_t** list = (edict_t**)Hunk_Alloc(sv.num_edicts * sizeof(edict_t*));

    int listcount = 0;
    SV_AreaTriggerEdicts(ent, sv_areanodes, list, &listcount, sv.num_edicts);

    const auto doTouch = [](edict_t* ent, edict_t* target) {
        const bool canBeTouched = quake::util::canBeTouched(target);

        if(!canBeTouched || !quake::util::entBoxIntersection(ent, target))
        {
            return;
        }

        const int old_self = pr_global_struct->self;
        const int old_other = pr_global_struct->other;

        pr_global_struct->self = EDICT_TO_PROG(target);
        pr_global_struct->other = EDICT_TO_PROG(ent);
        pr_global_struct->time = sv.time;

        if(target->v.touch)
        {
            PR_ExecuteProgram(target->v.touch);
        }

        // --------------------------------------------------------------------
        // VR: Simulate touching with right hand if body interactions are off.
        if(target->v.handtouch && vr_body_interactions.value == 1)
        {
            VR_SetFakeHandtouchParams(ent, target);
            PR_ExecuteProgram(target->v.handtouch);
        }
        // --------------------------------------------------------------------

        pr_global_struct->self = old_self;
        pr_global_struct->other = old_other;
    };

    const auto doHandtouch = [](edict_t* ent, edict_t* target) {
        // Add some size to the hands.
        const qfvec3 offsets{2.5f, 2.5f, 2.5f};

        const auto handposmin = ent->v.handpos - offsets;
        const auto handposmax = ent->v.handpos + offsets;
        const auto offhandposmin = ent->v.offhandpos - offsets;
        const auto offhandposmax = ent->v.offhandpos + offsets;

        const bool canBeHandTouched = quake::util::canBeHandTouched(target);

        const bool entIntersects =
            !quake::util::entBoxIntersection(ent, target);

        const auto intersects = [&](const qvec3& handMin,
                                    const qvec3& handMax) {
            // VR: This increases the boundaries for easier hand touching.
            const float bonus = quake::util::hasFlag(target, FL_EASYHANDTOUCH)
                                    ? VR_GetEasyHandTouchBonus()
                                    : 0.f;

            const qfvec3 bonusVec{bonus, bonus, bonus};

            return quake::util::boxIntersection(handMin, handMax,
                target->v.absmin - bonusVec, target->v.absmax + bonusVec);
        };

        const bool offHandIntersects = intersects(offhandposmin, offhandposmax);
        const bool mainHandIntersects = intersects(handposmin, handposmax);
        const bool anyHandIntersects = offHandIntersects || mainHandIntersects;

        const bool anyIntersection =
            vr_enabled.value ? anyHandIntersects : entIntersects;

        if(!canBeHandTouched || !anyIntersection)
        {
            return;
        }

        const int old_self = pr_global_struct->self;
        const int old_other = pr_global_struct->other;

        pr_global_struct->self = EDICT_TO_PROG(target);
        pr_global_struct->other = EDICT_TO_PROG(ent);
        pr_global_struct->time = sv.time;

        // VR: This is for things like ammo pickups and slipgates, and
        // dropped/thrown weapons.
        VR_SetHandtouchParams(
            offHandIntersects ? cVR_OffHand : cVR_MainHand, ent, target);
        PR_ExecuteProgram(target->v.handtouch);

        pr_global_struct->self = old_self;
        pr_global_struct->other = old_other;
    };

    for(int i = 0; i < listcount; i++)
    {
        edict_t* target = list[i]; // thing that's being touched
        // re-validate in case of PR_ExecuteProgram having side effects that
        // make edicts later in the list no longer touch

        if(target != ent)
        {
            doTouch(ent, target);

            if(quake::util::hasFlag(ent, FL_CLIENT))
            {
                doHandtouch(ent, target);
            }
        }
    }

    // free hunk-allocated edicts array
    Hunk_FreeToLowMark(mark);
}


/*
===============
SV_FindTouchedLeafs

===============
*/
void SV_FindTouchedLeafs(edict_t* ent, mnode_t* node)
{
    mplane_t* splitplane;
    mleaf_t* leaf;
    int sides;
    int leafnum;

    if(node->contents == CONTENTS_SOLID)
    {
        return;
    }

    // add an efrag if the node is a leaf

    if(node->contents < 0)
    {
        if(ent->num_leafs == MAX_ENT_LEAFS)
        {
            return;
        }

        leaf = (mleaf_t*)node;
        leafnum = leaf - sv.worldmodel->leafs - 1;

        ent->leafnums[ent->num_leafs] = leafnum;
        ent->num_leafs++;
        return;
    }

    // NODE_MIXED

    splitplane = node->plane;
    sides = BOX_ON_PLANE_SIDE(ent->v.absmin, ent->v.absmax, splitplane);

    // recurse down the contacted sides
    if(sides & 1)
    {
        SV_FindTouchedLeafs(ent, node->children[0]);
    }

    if(sides & 2)
    {
        SV_FindTouchedLeafs(ent, node->children[1]);
    }
}

/*
===============
SV_LinkEdict

===============
*/
void SV_LinkEdict(edict_t* ent, bool touch_triggers)
{
    if(ent->area.prev)
    {
        SV_UnlinkEdict(ent); // unlink from old position
    }

    if(ent == sv.edicts)
    {
        return; // don't add the world
    }

    if(ent->free)
    {
        return;
    }

    // set the abs box
    ent->v.absmin = ent->v.origin + ent->v.mins;
    ent->v.absmax = ent->v.origin + ent->v.maxs;

    //
    // to make items easier to pick up and allow them to be grabbed off
    // of shelves, the abs sizes are expanded
    //
    // TODO VR: (P2) interesting
    if(quake::util::hasFlag(ent, FL_ITEM))
    {
        ent->v.absmin[0] -= 15;
        ent->v.absmin[1] -= 15;
        ent->v.absmax[0] += 15;
        ent->v.absmax[1] += 15;
    }
    else if(quake::util::hasFlag(ent, FL_EASYHANDTOUCH))
    {
        const float bonus = VR_GetEasyHandTouchBonus();
        ent->v.absmin[0] -= bonus;
        ent->v.absmin[1] -= bonus;
        ent->v.absmax[0] += bonus;
        ent->v.absmax[1] += bonus;
    }
    else
    {
        // because movement is clipped an epsilon away from an actual edge,
        // we must fully check even when bounding boxes don't quite touch
        ent->v.absmin[0] -= 1;
        ent->v.absmin[1] -= 1;
        ent->v.absmin[2] -= 1;
        ent->v.absmax[0] += 1;
        ent->v.absmax[1] += 1;
        ent->v.absmax[2] += 1;
    }

    // link to PVS leafs
    ent->num_leafs = 0;
    if(ent->v.modelindex)
    {
        SV_FindTouchedLeafs(ent, sv.worldmodel->nodes);
    }

    if(ent->v.solid == SOLID_NOT)
    {
        return;
    }

    // find the first node that the ent's box crosses
    areanode_t* node = sv_areanodes;
    while(true)
    {
        if(node->axis == -1)
        {
            break;
        }

        if(ent->v.absmin[node->axis] > node->dist)
        {
            node = node->children[0];
        }
        else if(ent->v.absmax[node->axis] < node->dist)
        {
            node = node->children[1];
        }
        else
        {
            break; // crosses the node
        }
    }

    // link it in
    if(ent->v.solid == SOLID_TRIGGER)
    {
        InsertLinkBefore(&ent->area, &node->trigger_edicts);
    }
    else
    {
        InsertLinkBefore(&ent->area, &node->solid_edicts);
    }

    // if touch_triggers, touch all entities at this node and decend for more
    if(touch_triggers)
    {
        SV_TouchLinks(ent);
    }
}



/*
===============================================================================

POINT TESTING IN HULLS

===============================================================================
*/

/*
==================
SV_HullPointContents

==================
*/
int SV_HullPointContents(hull_t* hull, int num, const qvec3& p)
{
    while(num >= 0)
    {
        if(num < hull->firstclipnode || num > hull->lastclipnode)
        {
            Sys_Error("SV_HullPointContents: bad node number");
        }

        mclipnode_t* node = hull->clipnodes + num;
        mplane_t* plane = hull->planes + node->planenum;

        float d =
            plane->type < 3
                ? p[plane->type] - plane->dist
                : DoublePrecisionDotProduct(plane->normal, p) - plane->dist;

        num = d < 0 ? node->children[1] : node->children[0];
    }

    return num;
}


/*
==================
SV_PointContents

==================
*/
int SV_PointContents(const qvec3& p)
{
    const int cont = SV_HullPointContents(&sv.worldmodel->hulls[0], 0, p);
    if(cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN)
    {
        return CONTENTS_WATER;
    }

    return cont;
}

int SV_TruePointContents(const qvec3& p)
{
    return SV_HullPointContents(&sv.worldmodel->hulls[0], 0, p);
}

//===========================================================================

/*
============
SV_TestEntityPosition

This could be a lot more efficient...
============
*/
edict_t* SV_TestEntityPositionCustomOrigin(
    edict_t* ent, const qvec3& xOrigin)
{
    const trace_t trace =
        SV_Move(xOrigin, ent->v.mins, ent->v.maxs, xOrigin, MOVE_NORMAL, ent);

    return trace.startsolid ? sv.edicts : nullptr;
}

edict_t* SV_TestEntityPosition(edict_t* ent)
{
    return SV_TestEntityPositionCustomOrigin(ent, ent->v.origin);
}


/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/

/*
==================
SV_RecursiveHullCheck

==================
*/
bool SV_RecursiveHullCheck(hull_t* hull, int num, float p1f, float p2f,
    const qvec3& p1, const qvec3& p2, trace_t* trace)
{
    // ------------------------------------------------------------------------
    // VR: Solves weird crashes, probably related to hand pos on spawn.
    if(std::isnan(p1f) || std::isnan(p2f))
    {
        return false;
    }
    // ------------------------------------------------------------------------

    // check for empty
    if(num < 0)
    {
        if(num != CONTENTS_SOLID)
        {
            trace->allsolid = false;

            if(num == CONTENTS_EMPTY)
            {
                trace->inopen = true;
            }
            else
            {
                trace->inwater = true;
            }
        }
        else
        {
            trace->startsolid = true;
        }

        return true; // empty
    }

    if(num < hull->firstclipnode || num > hull->lastclipnode)
    {
        Sys_Error("SV_RecursiveHullCheck: bad node number");
    }

    //
    // find the point distances
    //
    mclipnode_t* node = hull->clipnodes + num; // johnfitz -- was dclipnode_t
    mplane_t* plane = hull->planes + node->planenum;

    float t1;
    float t2;
    if(plane->type < 3)
    {
        t1 = p1[plane->type] - plane->dist;
        t2 = p2[plane->type] - plane->dist;
    }
    else
    {
        t1 = DoublePrecisionDotProduct(plane->normal, p1) - plane->dist;
        t2 = DoublePrecisionDotProduct(plane->normal, p2) - plane->dist;
    }

    // ------------------------------------------------------------------------
    // VR: Solves weird crashes, probably related to hand pos on spawn.
    if(std::isnan(t1) || std::isnan(t2))
    {
        return false;
    }
    // ------------------------------------------------------------------------

#if 1
    if(t1 >= 0 && t2 >= 0)
    {
        return SV_RecursiveHullCheck(
            hull, node->children[0], p1f, p2f, p1, p2, trace);
    }

    if(t1 < 0 && t2 < 0)
    {
        return SV_RecursiveHullCheck(
            hull, node->children[1], p1f, p2f, p1, p2, trace);
    }
#else
    if((t1 >= DIST_EPSILON && t2 >= DIST_EPSILON) || (t2 > t1 && t1 >= 0))
        return SV_RecursiveHullCheck(
            hull, node->children[0], p1f, p2f, p1, p2, trace);

    if((t1 <= -DIST_EPSILON && t2 <= -DIST_EPSILON) || (t2 < t1 && t1 <= 0))
        return SV_RecursiveHullCheck(
            hull, node->children[1], p1f, p2f, p1, p2, trace);
#endif

    // put the crosspoint DIST_EPSILON pixels on the near side
    float frac;

    if(t1 < 0)
    {
        frac = (t1 + DIST_EPSILON) / (t1 - t2);
    }
    else
    {
        frac = (t1 - DIST_EPSILON) / (t1 - t2);
    }

    if(frac < 0)
    {
        frac = 0;
    }
    else if(frac > 1)
    {
        frac = 1;
    }

    qvec3 mid;
    float midf = p1f + (p2f - p1f) * frac;
    for(int i = 0; i < 3; i++)
    {
        mid[i] = p1[i] + frac * (p2[i] - p1[i]);
    }

    int side = (t1 < 0);

    // move up to the node
    if(!SV_RecursiveHullCheck(
           hull, node->children[side], p1f, midf, p1, mid, trace))
    {
        return false;
    }

#if 0
#ifdef PARANOID
    if(SV_HullPointContents(sv_hullmodel, mid, node->children[side]) ==
        CONTENTS_SOLID)
    {
        Con_Printf("mid PointInHullSolid\n");
        return false;
    }
#endif
#endif

    if(SV_HullPointContents(hull, node->children[side ^ 1], mid) !=
        CONTENTS_SOLID)
    {
        // go past the node
        return SV_RecursiveHullCheck(
            hull, node->children[side ^ 1], midf, p2f, mid, p2, trace);
    }

    if(trace->allsolid)
    {
        return false; // never got out of the solid area
    }

    //==================
    // the other side of the node is solid, this is the impact point
    //==================
    if(!side)
    {
        trace->plane.normal = plane->normal;
        trace->plane.dist = plane->dist;
    }
    else
    {
        trace->plane.normal = vec3_zero - plane->normal;
        trace->plane.dist = -plane->dist;
    }

    while(
        SV_HullPointContents(hull, hull->firstclipnode, mid) == CONTENTS_SOLID)
    {
        // shouldn't really happen, but does occasionally
        frac -= 0.1;
        if(frac < 0)
        {
            trace->fraction = midf;
            trace->endpos = mid;
            Con_DPrintf("backup past 0\n");
            return false;
        }
        midf = p1f + (p2f - p1f) * frac;
        for(int i = 0; i < 3; i++)
        {
            mid[i] = p1[i] + frac * (p2[i] - p1[i]);
        }
    }

    trace->fraction = midf;
    trace->endpos = mid;

    return false;
}

/*
==================
SV_ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t SV_ClipMoveToEntity(edict_t* ent, const qvec3& start,
    const qvec3& mins, const qvec3& maxs, const qvec3& end)
{
    // fill in a default trace
    trace_t trace;
    memset(&trace, 0, sizeof(trace_t));
    trace.fraction = 1;
    trace.allsolid = true;
    trace.endpos = end;

    // get the clipping hull
    qfvec3 offset;
    hull_t* hull = SV_HullForEntity(ent, mins, maxs, offset);

    const qvec3 start_l = start - qvec3(offset);
    const qvec3 end_l = end - qvec3(offset);

    // trace a line through the apropriate clipping hull
    SV_RecursiveHullCheck(
        hull, hull->firstclipnode, 0, 1, start_l, end_l, &trace);

    // fix trace up by the offset
    if(trace.fraction != 1)
    {
        trace.endpos += offset;
    }

    // did we clip the move?
    if(trace.fraction < 1 || trace.startsolid)
    {
        trace.ent = ent;
    }

    return trace;
}

//===========================================================================

/*
====================
SV_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
void SV_ClipToLinks(areanode_t* node, moveclip_t* clip)
{
    link_t* next;

    // touch linked edicts
    for(link_t* l = node->solid_edicts.next; l != &node->solid_edicts; l = next)
    {
        next = l->next;
        edict_t* target = EDICT_FROM_AREA(l);
        if(target->v.solid == SOLID_NOT ||
            target->v.solid == SOLID_NOT_BUT_TOUCHABLE ||
            target == clip->passedict)
        {
            continue;
        }

        if(target->v.solid == SOLID_TRIGGER)
        {
            Sys_Error("Trigger in clipping list");
        }

        if(clip->type == MOVE_NOMONSTERS && target->v.solid != SOLID_BSP)
        {
            continue;
        }

        if(!quake::util::boxIntersection(clip->boxmins, clip->boxmaxs,
               target->v.absmin, target->v.absmax))
        {
            continue;
        }

        if(clip->passedict && clip->passedict->v.size[0] && !target->v.size[0])
        {
            continue; // points never interact
        }

        // might intersect, so do an exact clip
        if(clip->trace.allsolid)
        {
            return;
        }

        if(clip->passedict)
        {
            if(PROG_TO_EDICT(target->v.owner) == clip->passedict)
            {
                continue; // don't clip against own missiles
            }

            if(PROG_TO_EDICT(clip->passedict->v.owner) == target)
            {
                continue; // don't clip against owner
            }
        }

        trace_t trace;
        if(quake::util::hasFlag(target, FL_MONSTER))
        {
            // VR: This branch here is also a hack in the original source code,
            // taken only for projectiles.

            trace = SV_ClipMoveToEntity(
                target, clip->start, clip->mins2, clip->maxs2, clip->end);
        }
        else
        {
            trace = SV_ClipMoveToEntity(
                target, clip->start, clip->mins, clip->maxs, clip->end);
        }

        if(trace.allsolid || trace.startsolid ||
            trace.fraction < clip->trace.fraction)
        {
            trace.ent = target;
            if(clip->trace.startsolid)
            {
                clip->trace = trace;
                clip->trace.startsolid = true;
            }
            else
            {
                clip->trace = trace;
            }
        }
        else if(trace.startsolid)
        {
            clip->trace.startsolid = true;
        }
    }

    // recurse down both sides
    if(node->axis == -1)
    {
        return;
    }

    if(clip->boxmaxs[node->axis] > node->dist)
    {
        SV_ClipToLinks(node->children[0], clip);
    }

    if(clip->boxmins[node->axis] < node->dist)
    {
        SV_ClipToLinks(node->children[1], clip);
    }
}


/*
==================
SV_MoveBounds
==================
*/
void SV_MoveBounds(const qvec3& start, const qvec3& mins,
    const qvec3& maxs, const qvec3& end, qvec3& boxmins,
    qvec3& boxmaxs)
{
#if 0
// debug to test against everything
boxmins[0] = boxmins[1] = boxmins[2] = -9999;
boxmaxs[0] = boxmaxs[1] = boxmaxs[2] = 9999;
#else
    for(int i = 0; i < 3; i++)
    {
        if(end[i] > start[i])
        {
            boxmins[i] = start[i] + mins[i] - 1;
            boxmaxs[i] = end[i] + maxs[i] + 1;
        }
        else
        {
            boxmins[i] = end[i] + mins[i] - 1;
            boxmaxs[i] = start[i] + maxs[i] + 1;
        }
    }
#endif
}

/*
==================
SV_Move
==================
*/
trace_t SV_Move(const qvec3& start, const qvec3& mins,
    const qvec3& maxs, const qvec3& end, const int type,
    edict_t* const passedict)
{
    moveclip_t clip;
    memset(&clip, 0, sizeof(moveclip_t));

    // clip to world
    clip.trace = SV_ClipMoveToEntity(sv.edicts, start, mins, maxs, end);

    clip.start = start;
    clip.end = end;
    clip.mins = mins;
    clip.maxs = maxs;
    clip.type = type;
    clip.passedict = passedict;

    if(type == MOVE_MISSILE)
    {
        // VR: This is an hardcoded hack to increase the size of projectiles
        // for the purpose of collision detection. It used to be 15, now it's
        // 2 to make aiming more important.
        for(int i = 0; i < 3; i++)
        {
            clip.mins2[i] = -2;
            clip.maxs2[i] = 2;
        }
    }
    else
    {
        clip.mins2 = mins;
        clip.maxs2 = maxs;
    }

    // create the bounding box of the entire move
    SV_MoveBounds(
        start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs);

    // clip to entities
    SV_ClipToLinks(sv_areanodes, &clip);

    return clip.trace;
}

/*
==================
SV_MoveTrace
==================
*/
trace_t SV_MoveTrace(const qvec3& start, const qvec3& end,
    const int type, edict_t* const passedict)
{
    return SV_Move(start, vec3_zero, vec3_zero, end, type, passedict);
}
