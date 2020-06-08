/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
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

#pragma once

#include "quakeglm_qvec3.hpp"
#include "protocol.hpp"

struct qmodel_t;
struct efrag_t;
struct mnode_t;

struct entity_t
{
    bool forcelink; // model changed

    int update_type;

    entity_state_t baseline; // to fill in defaults in updates

    double msgtime;       // time of last update
    qvec3 msg_origins[2]; // last two updates (0 is newest)
    qvec3 origin;
    qvec3 msg_angles[2]; // last two updates (0 is newest)
    qvec3 angles;
    qmodel_t* model;       // nullptr = no model
    efrag_t* efrag; // linked list of efrags
    int frame;
    float syncbase; // for client-side animations
    byte* colormap;
    int effects;  // light, particles, etc
    int skinnum;  // for Alias models
    int visframe; // last frame this entity was
                  //  found in an active leaf

    int dlightframe; // dynamic lighting
    int dlightbits;

    // FIXME: could turn these into a union
    int trivial_accept;
    mnode_t* topnode; // for bmodels, first world node
                             //  that splits bmodel, or nullptr if
                             //  not split

    byte alpha;         // johnfitz -- alpha
    byte lerpflags;     // johnfitz -- lerping
    float lerpstart;    // johnfitz -- animation lerping
    float lerptime;     // johnfitz -- animation lerping
    float lerpfinish;   // johnfitz -- lerping -- server sent us a more accurate
                        // interval, use it instead of 0.1
    short previouspose; // johnfitz -- animation lerping
    short currentpose;  // johnfitz -- animation lerping
    //	short					futurepose;		//johnfitz -- animation lerping
    float movelerpstart;  // johnfitz -- transform lerping
    qvec3 previousorigin; // johnfitz -- transform lerping
    qvec3 currentorigin;  // johnfitz -- transform lerping
    qvec3 previousangles; // johnfitz -- transform lerping
    qvec3 currentangles;  // johnfitz -- transform lerping

    bool horizFlip; // VR: horizontal flip

    // VR: per-instance scaling
    qvec3 msg_scales[2]; // last two updates (0 is newest)
    qvec3 scale;
    qvec3 scale_origin;

    bool hidden;      // TODO VR: (P1) hack? or document
    qfloat zeroBlend; // TODO VR: (P1) hack? or document
};

