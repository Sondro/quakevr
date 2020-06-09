#pragma once

#include "link.hpp"

struct areanode_t
{
    int axis; // -1 = leaf node
    float dist;
    areanode_t* children[2];
    link_t trigger_edicts;
    link_t solid_edicts;
};

#define AREA_DEPTH 4
#define AREA_NODES 32

// TODO VR: (P0) QSS Merge
#if 1
static areanode_t sv_areanodes[AREA_NODES];
static int sv_numareanodes;
#endif
