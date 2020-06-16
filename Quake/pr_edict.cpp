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
// sv_edict.c -- entity dictionary

#include "host.hpp"
#include "quakedef.hpp"
#include "cmd.hpp"
#include "console.hpp"
#include "crc.hpp"
#include "mathlib.hpp"
#include "zone.hpp"
#include "byteorder.hpp"
#include "progs.hpp"
#include "pr_comp.hpp"
#include "server.hpp"
#include "world.hpp"

#include <cassert>

dprograms_t* progs;
dfunction_t* pr_functions;

bool pr_alpha_supported; // johnfitz

dstatement_t* pr_statements;
globalvars_t* pr_global_struct;
float* pr_globals; // same as pr_global_struct
int pr_edict_size; // in bytes

unsigned short pr_crc;

int type_size[8] = {
    1, // ev_void
    1, // sizeof(string_t) / 4		// ev_string
    1, // ev_float
    3, // ev_vector
    1, // ev_entity
    1, // ev_field
    1, // sizeof(func_t) / 4		// ev_function
    1  // sizeof(void *) / 4		// ev_pointer
};

static ddef_t* ED_FieldAtOfs(int ofs);
static bool ED_ParseEpair(void* base, ddef_t* key, const char* s);

#define MAX_FIELD_LEN 64
#define GEFV_CACHESIZE 2

typedef struct
{
    ddef_t* pcache;
    char field[MAX_FIELD_LEN];
} gefv_cache;

static gefv_cache gefvCache[GEFV_CACHESIZE] = {{nullptr, ""}, {nullptr, ""}};

cvar_t nomonsters = {"nomonsters", "0", CVAR_NONE};
cvar_t gamecfg = {"gamecfg", "0", CVAR_NONE};
cvar_t scratch1 = {"scratch1", "0", CVAR_NONE};
cvar_t scratch2 = {"scratch2", "0", CVAR_NONE};
cvar_t scratch3 = {"scratch3", "0", CVAR_NONE};
cvar_t scratch4 = {"scratch4", "0", CVAR_NONE};
cvar_t savedgamecfg = {"savedgamecfg", "0", CVAR_ARCHIVE};
cvar_t saved1 = {"saved1", "0", CVAR_ARCHIVE};
cvar_t saved2 = {"saved2", "0", CVAR_ARCHIVE};
cvar_t saved3 = {"saved3", "0", CVAR_ARCHIVE};
cvar_t saved4 = {"saved4", "0", CVAR_ARCHIVE};

/*
=================
ED_ClearEdict

Sets everything to nullptr
=================
*/
void ED_ClearEdict(edict_t* e)
{
    memset(&e->v, 0, qcvm->progs->entityfields * 4);
    e->free = false;
}

/*
=================
ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t* ED_Alloc()
{
    int i;
    edict_t* e;

    for(i = svs.maxclients + 1; i < qcvm->num_edicts; i++)
    {
        e = EDICT_NUM(i);
        // the first couple seconds of server time can involve a lot of
        // freeing and allocating, so relax the replacement policy
        if(e->free && (e->freetime < 2 || qcvm->time - e->freetime > 0.5))
        {
            ED_ClearEdict(e);
            return e;
        }
    }

    if(i == qcvm->max_edicts)
    {
        // johnfitz -- use qcvm->max_edicts instead of
        // MAX_EDICTS
        Host_Error(
            "ED_Alloc: no free edicts (max_edicts is %i)", qcvm->max_edicts);
    }

    qcvm->num_edicts++;
    e = EDICT_NUM(i);
    memset(
        e, 0, qcvm->edict_size); // ericw -- switched sv.edicts to malloc(), so
                                 // we are accessing uninitialized memory and
                                 // must fully zero it, not just ED_ClearEdict

    return e;
}

/*
=================
ED_Free

Marks the edict as free
FIXME: walk all entities and nullptr out references to this entity
=================
*/
void ED_Free(edict_t* ed)
{
    SV_UnlinkEdict(ed); // unlink from world bsp

    ed->free = true;
    ed->v.model = 0;
    ed->v.takedamage = 0;
    ed->v.modelindex = 0;
    ed->v.colormap = 0;
    ed->v.skin = 0;
    ed->v.frame = 0;
    ed->v.origin = vec3_zero;
    ed->v.angles = vec3_zero;
    ed->v.nextthink = -1;
    ed->v.nextthink2 = -1;
    ed->v.solid = 0;
    ed->alpha = ENTALPHA_DEFAULT; // johnfitz -- reset alpha for next entity

    ed->freetime = qcvm->time;
}

//===========================================================================

/*
============
ED_GlobalAtOfs
============
*/
static ddef_t* ED_GlobalAtOfs(int ofs)
{
    ddef_t* def;
    int i;

    for(i = 0; i < qcvm->progs->numglobaldefs; i++)
    {
        def = &qcvm->globaldefs[i];
        if(def->ofs == ofs)
        {
            return def;
        }
    }
    return nullptr;
}

/*
============
ED_FieldAtOfs
============
*/
static ddef_t* ED_FieldAtOfs(int ofs)
{
    ddef_t* def;
    int i;

    for(i = 0; i < qcvm->progs->numfielddefs; i++)
    {
        def = &qcvm->fielddefs[i];
        if(def->ofs == ofs)
        {
            return def;
        }
    }
    return nullptr;
}

/*
============
ED_FindField
============
*/
ddef_t* ED_FindField(const char* name)
{
    ddef_t* def;
    int i;

    for(i = 0; i < qcvm->progs->numfielddefs; i++)
    {
        def = &qcvm->fielddefs[i];
        if(!strcmp(PR_GetString(def->s_name), name))
        {
            return def;
        }
    }
    return nullptr;
}

/*
 */
int ED_FindFieldOffset(const char* name)
{
    ddef_t* def = ED_FindField(name);
    if(!def)
    {
        return -1;
    }
    return def->ofs;
}

/*
============
ED_FindGlobal
============
*/
ddef_t* ED_FindGlobal(const char* name)
{
    ddef_t* def;
    int i;

    for(i = 0; i < qcvm->progs->numglobaldefs; i++)
    {
        def = &qcvm->globaldefs[i];
        if(!strcmp(PR_GetString(def->s_name), name))
        {
            return def;
        }
    }
    return nullptr;
}


/*
============
ED_FindFunction
============
*/
dfunction_t* ED_FindFunction(const char* fn_name)
{
    dfunction_t* func;
    int i;

    for(i = 0; i < qcvm->progs->numfunctions; i++)
    {
        func = &qcvm->functions[i];
        if(!strcmp(PR_GetString(func->s_name), fn_name))
        {
            return func;
        }
    }
    return nullptr;
}

/*
============
GetEdictFieldValue
============
*/
eval_t* GetEdictFieldValue(edict_t* ed, int fldofs)
    {
    if(fldofs < 0)
    {
        return nullptr;
    }

    return (eval_t*)((char*)&ed->v + fldofs * 4);
}


/*
============
PR_ValueString
(etype_t type, eval_t *val)

Returns a string describing *data in a type specific manner
=============
*/
static const char* PR_ValueString(int type, eval_t* val)
{
    static char line[512];
    ddef_t* def;
    dfunction_t* f;

    type &= ~DEF_SAVEGLOBAL;

    switch(type)
    {
        case ev_string: sprintf(line, "%s", PR_GetString(val->string)); break;
        case ev_entity:
            sprintf(
                line, "entity %i", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
            break;
        case ev_function:
            f = qcvm->functions + val->function;
            sprintf(line, "%s()", PR_GetString(f->s_name));
            break;
        case ev_field:
            def = ED_FieldAtOfs(val->_int);
            sprintf(line, ".%s", PR_GetString(def->s_name));
            break;
        case ev_void: sprintf(line, "void"); break;
        case ev_float: sprintf(line, "%5.1f", val->_float); break;
        case ev_ext_integer: sprintf(line, "%i", val->_int); break;
        case ev_vector:
            sprintf(line, "'%5.1f %5.1f %5.1f'", val->vector[0], val->vector[1],
                val->vector[2]);
            break;
        case ev_pointer: sprintf(line, "pointer"); break;
        default: sprintf(line, "bad type %i", type); break;
    }

    return line;
}

/*
============
PR_UglyValueString
(etype_t type, eval_t *val)

Returns a string describing *data in a type specific manner
Easier to parse than PR_ValueString
=============
*/
const char* PR_UglyValueString(int type, eval_t* val)
{
    static char line[1024];
    ddef_t* def;
    dfunction_t* f;

    type &= ~DEF_SAVEGLOBAL;

    switch(type)
    {
        case ev_string:
            q_snprintf(line, sizeof(line), "%s", PR_GetString(val->string));
            break;
        case ev_entity:
            q_snprintf(line, sizeof(line), "%i",
                NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
            break;
        case ev_function:
            f = qcvm->functions + val->function;
            q_snprintf(line, sizeof(line), "%s", PR_GetString(f->s_name));
            break;
        case ev_field:
            def = ED_FieldAtOfs(val->_int);
            q_snprintf(line, sizeof(line), "%s", PR_GetString(def->s_name));
            break;
        case ev_void: q_snprintf(line, sizeof(line), "void"); break;
        case ev_float: q_snprintf(line, sizeof(line), "%f", val->_float); break;
        case ev_ext_integer: sprintf(line, "%i", val->_int); break;
        case ev_vector:
            q_snprintf(line, sizeof(line), "%f %f %f", val->vector[0],
                val->vector[1], val->vector[2]);
            break;
        default: q_snprintf(line, sizeof(line), "bad type %i", type); break;
    }

    return line;
}

/*
============
PR_GlobalString

Returns a string with a description and the contents of a global,
padded to 20 field width
============
*/
const char* PR_GlobalString(int ofs)
{
    static char line[512];
    const char* s;
    int i;
    ddef_t* def;
    void* val;

    val = (void*)&qcvm->globals[ofs];
    def = ED_GlobalAtOfs(ofs);
    if(!def)
    {
        sprintf(line, "%i(?)", ofs);
    }
    else
    {
        s = PR_ValueString(def->type, (eval_t*)val);
        sprintf(line, "%i(%s)%s", ofs, PR_GetString(def->s_name), s);
    }

    i = strlen(line);
    for(; i < 20; i++)
    {
        strcat(line, " ");
    }
    strcat(line, " ");

    return line;
}

const char* PR_GlobalStringNoContents(int ofs)
{
    static char line[512];
    int i;
    ddef_t* def;

    def = ED_GlobalAtOfs(ofs);
    if(!def)
    {
        sprintf(line, "%i(?)", ofs);
    }
    else
    {
        sprintf(line, "%i(%s)", ofs, PR_GetString(def->s_name));
    }

    i = strlen(line);
    for(; i < 20; i++)
    {
        strcat(line, " ");
    }
    strcat(line, " ");

    return line;
}


/*
=============
ED_Print

For debugging
=============
*/
void ED_Print(edict_t* ed)
{
    ddef_t* d;
    int* v;
    int i;
    int j;
    int l;
    const char* name;
    int type;

    if(ed->free)
    {
        Con_Printf("FREE\n");
        return;
    }

    Con_SafePrintf(
        "\nEDICT %i:\n", NUM_FOR_EDICT(ed)); // johnfitz -- was Con_Printf
    for(i = 1; i < qcvm->progs->numfielddefs; i++)
    {
        d = &qcvm->fielddefs[i];
        name = PR_GetString(d->s_name);
        l = strlen(name);
        if(l > 1 && name[l - 2] == '_')
        {
            continue; // skip _x, _y, _z vars
        }

        v = (int*)((char*)&ed->v + d->ofs * 4);

        // if the value is still all 0, skip the field
        type = d->type & ~DEF_SAVEGLOBAL;

        for(j = 0; j < type_size[type]; j++)
        {
            if(v[j])
            {
                break;
            }
        }
        if(j == type_size[type])
        {
            continue;
        }

        Con_SafePrintf("%s", name); // johnfitz -- was Con_Printf
        while(l++ < 15)
        {
            Con_SafePrintf(" "); // johnfitz -- was Con_Printf
        }

        Con_SafePrintf("%s\n",
            PR_ValueString(d->type, (eval_t*)v)); // johnfitz -- was Con_Printf
    }
}

/*
=============
ED_Write

For savegames
=============
*/
void ED_Write(FILE* f, edict_t* ed)
{
    ddef_t* d;
    int* v;
    int i;
    int j;
    const char* name;
    int type;

    fprintf(f, "{\n");

    if(ed->free)
    {
        fprintf(f, "}\n");
        return;
    }

    for(i = 1; i < qcvm->progs->numfielddefs; i++)
    {
        d = &qcvm->fielddefs[i];
        name = PR_GetString(d->s_name);
        j = strlen(name);
        if(j > 1 && name[j - 2] == '_')
        {
            continue; // skip _x, _y, _z vars
        }

        v = (int*)((char*)&ed->v + d->ofs * 4);

        // if the value is still all 0, skip the field
        type = d->type & ~DEF_SAVEGLOBAL;
        for(j = 0; j < type_size[type]; j++)
        {
            if(v[j])
            {
                break;
            }
        }
        if(j == type_size[type])
        {
            continue;
        }

        fprintf(f, "\"%s\" ", name);
        fprintf(f, "\"%s\"\n", PR_UglyValueString(d->type, (eval_t*)v));
    }

    // johnfitz -- save entity alpha manually when progs.dat doesn't know about
    // alpha
    if(!pr_alpha_supported && ed->alpha != ENTALPHA_DEFAULT)
    {
        fprintf(f, "\"alpha\" \"%f\"\n", ENTALPHA_TOSAVE(ed->alpha));
    }
    // johnfitz

    fprintf(f, "}\n");
}

void ED_PrintNum(int ent)
{
    ED_Print(EDICT_NUM(ent));
}

/*
=============
ED_PrintEdicts

For debugging, prints all the entities in the current server
=============
*/
void ED_PrintEdicts()
{
    int i;

    if(!sv.active)
    {
        return;
    }

    PR_SwitchQCVM(&sv.qcvm);
    Con_Printf("%i entities\n", qcvm->num_edicts);
    for(i = 0; i < qcvm->num_edicts; i++)
    {
        ED_PrintNum(i);
    }
    PR_SwitchQCVM(nullptr);
}

/*
=============
ED_PrintEdict_f

For debugging, prints a single edicy
=============
*/
static void ED_PrintEdict_f()
{
    int i;

    if(!sv.active)
    {
        return;
    }

    i = Q_atoi(Cmd_Argv(1));
    PR_SwitchQCVM(&sv.qcvm);
    if(i < 0 || i >= qcvm->num_edicts)
    {
        Con_Printf("Bad edict number\n");
    }
    else
    {
        if(Cmd_Argc() == 2 || svs.maxclients != 1)
        { // edict N
    ED_PrintNum(i);
}
        else // edict N FLD ...
        {
            ddef_t* def = ED_FindField(Cmd_Argv(2));
            if(!def)
            {
                Con_Printf("Field %s not defined\n", Cmd_Argv(2));
            }
            else if(Cmd_Argc() < 4)
            {
                Con_Printf("Edict %u.%s==%s\n", i, PR_GetString(def->s_name),
                    PR_UglyValueString(def->type & ~DEF_SAVEGLOBAL,
                        (eval_t*)((char*)&EDICT_NUM(i)->v + def->ofs * 4)));
            }
            else
            {
                ED_ParseEpair((void*)&EDICT_NUM(i)->v, def, Cmd_Argv(3));
            }
        }
    }
    PR_SwitchQCVM(nullptr);
}

/*
=============
ED_Count

For debugging
=============
*/
static void ED_Count()
{
    edict_t* ent;
    int i;
    int active;
    int models;
    int solid;
    int step;

    if(!sv.active)
    {
        return;
    }

    PR_SwitchQCVM(&sv.qcvm);
    active = models = solid = step = 0;
    for(i = 0; i < qcvm->num_edicts; i++)
    {
        ent = EDICT_NUM(i);
        if(ent->free)
        {
            continue;
        }
        active++;
        if(ent->v.solid)
        {
            solid++;
        }
        if(ent->v.model)
        {
            models++;
        }
        if(ent->v.movetype == MOVETYPE_STEP)
        {
            step++;
        }
    }

    Con_Printf("num_edicts:%3i\n", qcvm->num_edicts);
    Con_Printf("active    :%3i\n", active);
    Con_Printf("view      :%3i\n", models);
    Con_Printf("touch     :%3i\n", solid);
    Con_Printf("step      :%3i\n", step);
    PR_SwitchQCVM(nullptr);
}


/*
==============================================================================

ARCHIVING GLOBALS

FIXME: need to tag constants, doesn't really work
==============================================================================
*/

/*
=============
ED_WriteGlobals
=============
*/
void ED_WriteGlobals(FILE* f)
{
    ddef_t* def;
    int i;
    const char* name;
    int type;

    fprintf(f, "{\n");
    for(i = 0; i < qcvm->progs->numglobaldefs; i++)
    {
        def = &qcvm->globaldefs[i];
        type = def->type;
        if(!(def->type & DEF_SAVEGLOBAL))
        {
            continue;
        }
        type &= ~DEF_SAVEGLOBAL;

        if(type != ev_string && type != ev_float && type != ev_ext_integer &&
            type != ev_entity)
        {
            continue;
        }

        name = PR_GetString(def->s_name);
        fprintf(f, "\"%s\" ", name);
        fprintf(f, "\"%s\"\n",
            PR_UglyValueString(type, (eval_t*)&qcvm->globals[def->ofs]));
    }
    fprintf(f, "}\n");
}

/*
=============
ED_ParseGlobals
=============
*/
const char* ED_ParseGlobals(const char* data)
{
    char keyname[64];
    ddef_t* key;

    while(true)
    {
        // parse key
        data = COM_Parse(data);
        if(com_token[0] == '}')
        {
            break;
        }
        if(!data)
        {
            Host_Error("ED_ParseEntity: EOF without closing brace");
        }

        q_strlcpy(keyname, com_token, sizeof(keyname));

        // parse value
        data = COM_Parse(data);
        if(!data)
        {
            Host_Error("ED_ParseEntity: EOF without closing brace");
        }

        if(com_token[0] == '}')
        {
            Host_Error("ED_ParseEntity: closing brace without data");
        }

        key = ED_FindGlobal(keyname);
        if(!key)
        {
            Con_Printf("'%s' is not a global\n", keyname);
            continue;
        }

        if(!ED_ParseEpair((void*)qcvm->globals, key, com_token))
        {
            Host_Error("ED_ParseGlobals: parse error");
        }
    }
    return data;
}

//============================================================================


/*
=============
ED_NewString
=============
*/
static string_t ED_NewString(const char* string)
{
    char* new_p;
    int i;
    int l;
    string_t num;

    l = strlen(string) + 1;
    num = PR_AllocString(l, &new_p);

    for(i = 0; i < l; i++)
    {
        if(string[i] == '\\' && i < l - 1)
        {
            i++;
            if(string[i] == 'n')
            {
                *new_p++ = '\n';
            }
            else
            {
                *new_p++ = '\\';
            }
        }
        else
        {
            *new_p++ = string[i];
        }
    }

    return num;
}


/*
=============
ED_ParseEval

Can parse either fields or globals
returns false if error
=============
*/
bool ED_ParseEpair(void* base, ddef_t* key, const char* s)
{
    int i;
    char string[128];
    ddef_t* def;
    char* v;
    char* w;
    char* end;
    void* d;
    dfunction_t* func;

    d = (void*)((int*)base + key->ofs);

    switch(key->type & ~DEF_SAVEGLOBAL)
    {
        case ev_string: *(string_t*)d = ED_NewString(s); break;

        case ev_float: *(float*)d = atof(s); break;

        case ev_ext_integer: *(int*)d = atoi(s); break;

        case ev_vector:
            q_strlcpy(string, s, sizeof(string));
            end = (char*)string + strlen(string);
            v = string;
            w = string;

            for(i = 0; i < 3 && (w <= end);
                i++) // ericw -- added (w <= end) check
            {
                // set v to the next space (or 0 byte), and change that char to
                // a 0 byte
                while(*v && *v != ' ')
                {
                    v++;
                }
                *v = 0;
                ((float*)d)[i] = atof(w);
                w = v = v + 1;
            }
            // ericw -- fill remaining elements to 0 in case we hit the end of
            // string before reading 3 floats.
            if(i < 3)
            {
                Con_DWarning("Avoided reading garbage for \"%s\" \"%s\"\n",
                    PR_GetString(key->s_name), s);
                for(; i < 3; i++)
                {
                    ((float*)d)[i] = 0.0f;
                }
            }
            break;

        case ev_entity:
            if(!strncmp(s, "entity ", 7))
            { // Spike: putentityfieldstring/etc should
                // be able to cope with etos's weirdness.
                s += 7;
            }
            *(int*)d = EDICT_TO_PROG(EDICT_NUM(atoi(s)));
            break;

        case ev_field:
            def = ED_FindField(s);
            if(!def)
            {
                // johnfitz -- HACK -- suppress error becuase fog/sky fields
                // might not be mentioned in defs.qc
                if(strncmp(s, "sky", 3) && strcmp(s, "fog"))
                {
                    Con_DPrintf("Can't find field %s\n", s);
                }
                return false;
            }
            *(int*)d = G_INT(def->ofs);
            break;

        case ev_function:
            func = ED_FindFunction(s);
            if(!func)
            {
                Con_Printf("Can't find function %s\n", s);
                return false;
            }
            *(func_t*)d = func - qcvm->functions;
            break;

        default: break;
    }
    return true;
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
Used for initial level load and for savegames.
====================
*/
const char* ED_ParseEdict(const char* data, edict_t* ent)
{
    bool init = false;

    // clear it
    if(ent != qcvm->edicts)
    {
    	// hack
        memset(&ent->v, 0, qcvm->progs->entityfields * 4);
    }

    // go through all the dictionary pairs
    while(true)
    {
        // parse key
        data = COM_Parse(data);

        if(com_token[0] == '}')
        {
            break;
        }

        if(!data)
        {
            Host_Error("ED_ParseEntity: EOF without closing brace");
        }

        // anglehack is to allow QuakeEd to write single scalar angles
        // and allow them to be turned into vectors. (FIXME...)
        bool anglehack;
        if(!strcmp(com_token, "angle"))
        {
            strcpy(com_token, "angles");
            anglehack = true;
        }
        else
        {
            anglehack = false;
        }

        // FIXME: change light to _light to get rid of this hack
        if(!strcmp(com_token, "light"))
        {
            strcpy(com_token, "light_lev"); // hack for single light def
        }

        char keyname[256];
        q_strlcpy(keyname, com_token, sizeof(keyname));

        // another hack to fix keynames with trailing spaces
        int n = strlen(keyname);
        while(n && keyname[n - 1] == ' ')
        {
            keyname[n - 1] = 0;
            n--;
        }

        // parse value
        data = COM_Parse(data);
        if(!data)
        {
            Host_Error("ED_ParseEntity: EOF without closing brace");
        }

        if(com_token[0] == '}')
        {
            Host_Error("ED_ParseEntity: closing brace without data");
        }

        init = true;

        // keynames with a leading underscore are used for utility comments,
        // and are immediately discarded by quake
        if(keyname[0] == '_')
        {
            continue;
        }

        // johnfitz -- hack to support .alpha even when progs.dat doesn't know
        // about it
        if(!strcmp(keyname, "alpha"))
        {
            ent->alpha = ENTALPHA_ENCODE(atof(com_token));
        }
        // johnfitz

        ddef_t* key = ED_FindField(keyname);
        if(!key)
        {
#ifdef PSET_SCRIPT
            eval_t* val;
            if(!strcmp(keyname, "traileffect") && sv.state == ss_loading)
            {
                if((val = GetEdictFieldValue(
                        ent, qcvm->extfields.traileffectnum)))
                {
                    val->_float = PF_SV_ForceParticlePrecache(com_token);
                }
            }
            else if(!strcmp(keyname, "emiteffect") && sv.state == ss_loading)
            {
                if((val = GetEdictFieldValue(
                        ent, qcvm->extfields.emiteffectnum)))
                {
                    val->_float = PF_SV_ForceParticlePrecache(com_token);
                }
            }
            // johnfitz -- HACK -- suppress error becuase fog/sky/alpha fields
            // might not be mentioned in defs.qc
            else
#endif
            if(strncmp(keyname, "sky", 3) && strcmp(keyname, "fog") &&
                strcmp(keyname, "alpha"))
            {
                Con_DPrintf("\"%s\" is not a field\n",
                    keyname); // johnfitz -- was Con_Printf
            }

            continue;
        }

        if(anglehack)
        {
            char temp[32];
            strcpy(temp, com_token);
            sprintf(com_token, "0 %s 0", temp);
        }

        if(!ED_ParseEpair((void*)&ent->v, key, com_token))
        {
            Host_Error("ED_ParseEdict: parse error");
        }
    }

    if(!init)
    {
        ent->free = true;
    }

    return data;
}


/*
================
ED_LoadFromFile

The entities are directly placed in the array, rather than allocated with
ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.

Used for both fresh maps and savegame loads.  A fresh map would also need
to call ED_CallSpawnFunctions () to let the objects initialize themselves.
================
*/
void ED_LoadFromFile(const char* data)
{
    edict_t* ent = nullptr;
    int inhibit = 0;
    int usingspawnfunc = 0;

    pr_global_struct->time = qcvm->time;

    // parse ents
    while(true)
    {
        // parse the opening brace
        data = COM_Parse(data);

        if(!data)
        {
            break;
        }

        if(com_token[0] != '{')
        {
            Host_Error("ED_LoadFromFile: found %s when expecting {", com_token);
        }

        ent = (!ent) ? EDICT_NUM(0) : ED_Alloc();
        data = ED_ParseEdict(data, ent);

        // remove things from different skill levels or deathmatch
        if(deathmatch.value)
        {
            if(((int)ent->v.spawnflags & SPAWNFLAG_NOT_DEATHMATCH))
            {
                ED_Free(ent);
                inhibit++;
                continue;
            }
        }
        else if((current_skill == 0 &&
                    ((int)ent->v.spawnflags & SPAWNFLAG_NOT_EASY)) ||
                (current_skill == 1 &&
                    ((int)ent->v.spawnflags & SPAWNFLAG_NOT_MEDIUM)) ||
                (current_skill >= 2 &&
                    ((int)ent->v.spawnflags & SPAWNFLAG_NOT_HARD)))
        {
            ED_Free(ent);
            inhibit++;
            continue;
        }

        //
        // immediately call spawn function
        //
        assert(ent != nullptr);
        if(!ent->v.classname)
        {
            Con_SafePrintf("No classname for:\n"); // johnfitz -- was Con_Printf
            ED_Print(ent);
            ED_Free(ent);
            continue;
        }

        // look for the spawn function
        //
        dfunction_t* func =
            ED_FindFunction(va("spawnfunc_%s", PR_GetString(ent->v.classname)));
        if(func)
        {
            if(!usingspawnfunc++)
            {
                Con_DPrintf2("Using DP_SV_SPAWNFUNC_PREFIX\n");
            }
        }
        else
        {
            func = ED_FindFunction(PR_GetString(ent->v.classname));
        }

        if(!func)
        {
            const char* classname = PR_GetString(ent->v.classname);
            if(!strcmp(classname, "misc_model"))
            {
                PR_spawnfunc_misc_model(ent);
            }
            else
            {
            Con_SafePrintf(
                "No spawn function for:\n"); // johnfitz -- was Con_Printf
            ED_Print(ent);
            ED_Free(ent);
            }
            continue;
        }

        pr_global_struct->self = EDICT_TO_PROG(ent);
        PR_ExecuteProgram(func - qcvm->functions);
    }

    Con_DPrintf("%i entities inhibited\n", inhibit);
}


/*
===============
PR_LoadProgs
===============
*/
void PR_LoadProgs()
{
    int i;

    // flush the non-C variable lookup cache
    for(i = 0; i < GEFV_CACHESIZE; i++)
    {
        gefvCache[i].field[0] = 0;
    }

    CRC_Init(&pr_crc);

    progs = (dprograms_t*)COM_LoadHunkFile("vrprogs.dat", nullptr);
    if(!progs)
    {
        Host_Error("PR_LoadProgs: couldn't load vrprogs.dat");
    }
    Con_DPrintf("Programs occupy %iK.\n", com_filesize / 1024);

    for(i = 0; i < com_filesize; i++)
    {
        CRC_ProcessByte(&pr_crc, ((byte*)progs)[i]);
    }

    // byte swap the header
    for(i = 0; i < (int)sizeof(*progs) / 4; i++)
    {
        ((int*)progs)[i] = LittleLong(((int*)progs)[i]);
    }

    if(progs->version != PROG_VERSION)
    {
        Host_Error("vrprogs.dat has wrong version number (%i should be %i)",
            progs->version, PROG_VERSION);
    }

    if(progs->crc != PROGHEADER_CRC)
    {
        Host_Error(
            "vrprogs.dat system vars have been modified, progdefs.h is out of "
            "date");
    }

    pr_functions = (dfunction_t*)((byte*)progs + progs->ofs_functions);
    qcvm->strings = (char*)progs + progs->ofs_strings;
    if(progs->ofs_strings + progs->numstrings >= com_filesize)
    {
        Host_Error("vrprogs.dat strings go past end of file\n");
    }

    // initialize the strings
    qcvm->numknownstrings = 0;
    qcvm->maxknownstrings = 0;
    qcvm->stringssize = progs->numstrings;
    if(qcvm->knownstrings)
    {
        Z_Free((void*)qcvm->knownstrings);
    }
    qcvm->knownstrings = nullptr;
    PR_SetEngineString("");

    qcvm->globaldefs = (ddef_t*)((byte*)progs + progs->ofs_globaldefs);
    qcvm->fielddefs = (ddef_t*)((byte*)progs + progs->ofs_fielddefs);
    pr_statements = (dstatement_t*)((byte*)progs + progs->ofs_statements);

    pr_global_struct = (globalvars_t*)((byte*)progs + progs->ofs_globals);
    qcvm->globals = (float*)pr_global_struct;

    // byte swap the lumps
    for(i = 0; i < qcvm->progs->numstatements; i++)
    {
        qcvm->statements[i].op = LittleShort(qcvm->statements[i].op);
        qcvm->statements[i].a = LittleShort(qcvm->statements[i].a);
        qcvm->statements[i].b = LittleShort(qcvm->statements[i].b);
        qcvm->statements[i].c = LittleShort(qcvm->statements[i].c);
    }

    for(i = 0; i < qcvm->progs->numfunctions; i++)
    {
        qcvm->functions[i].first_statement =
            LittleLong(qcvm->functions[i].first_statement);
        qcvm->functions[i].parm_start =
            LittleLong(qcvm->functions[i].parm_start);
        qcvm->functions[i].s_name = LittleLong(qcvm->functions[i].s_name);
        qcvm->functions[i].s_file = LittleLong(qcvm->functions[i].s_file);
        qcvm->functions[i].numparms = LittleLong(qcvm->functions[i].numparms);
        qcvm->functions[i].locals = LittleLong(qcvm->functions[i].locals);
    }

    for(i = 0; i < qcvm->progs->numglobaldefs; i++)
    {
        qcvm->globaldefs[i].type = LittleShort(qcvm->globaldefs[i].type);
        qcvm->globaldefs[i].ofs = LittleShort(qcvm->globaldefs[i].ofs);
        qcvm->globaldefs[i].s_name = LittleLong(qcvm->globaldefs[i].s_name);
    }

    pr_alpha_supported = false; // johnfitz

    for(i = 0; i < qcvm->progs->numfielddefs; i++)
    {
        qcvm->fielddefs[i].type = LittleShort(qcvm->fielddefs[i].type);
        if(qcvm->fielddefs[i].type & DEF_SAVEGLOBAL)
        {
            Host_Error("PR_LoadProgs: pr_fielddefs[i].type & DEF_SAVEGLOBAL");
        }
        qcvm->fielddefs[i].ofs = LittleShort(qcvm->fielddefs[i].ofs);
        qcvm->fielddefs[i].s_name = LittleLong(qcvm->fielddefs[i].s_name);

        // johnfitz -- detect alpha support in vrprogs.dat
        if(!strcmp(qcvm->strings + qcvm->fielddefs[i].s_name, "alpha"))
        {
            pr_alpha_supported = true;
        }
        // johnfitz
    }

    for(i = 0; i < progs->numglobals; i++)
    {
        ((int*)qcvm->globals)[i] = LittleLong(((int*)qcvm->globals)[i]);
    }

    pr_edict_size =
        progs->entityfields * 4 + sizeof(edict_t) - sizeof(entvars_t);
    // round off to next highest whole word address (esp for Alpha)
    // this ensures that pointers in the engine data area are always
    // properly aligned
    qcvm->edict_size += sizeof(void*) - 1;
    qcvm->edict_size &= ~(sizeof(void*) - 1);
}


/*
===============
PR_Init
===============
*/
void PR_Init()
{
    Cmd_AddCommand("edict", ED_PrintEdict_f);
    Cmd_AddCommand("edicts", ED_PrintEdicts);
    Cmd_AddCommand("edictcount", ED_Count);
    Cmd_AddCommand("profile", PR_Profile_f);
    Cvar_RegisterVariable(&nomonsters);
    Cvar_RegisterVariable(&gamecfg);
    Cvar_RegisterVariable(&scratch1);
    Cvar_RegisterVariable(&scratch2);
    Cvar_RegisterVariable(&scratch3);
    Cvar_RegisterVariable(&scratch4);
    Cvar_RegisterVariable(&savedgamecfg);
    Cvar_RegisterVariable(&saved1);
    Cvar_RegisterVariable(&saved2);
    Cvar_RegisterVariable(&saved3);
    Cvar_RegisterVariable(&saved4);
}


edict_t* EDICT_NUM(int n)
{
    if(n < 0 || n >= qcvm->max_edicts)
    {
        Host_Error("EDICT_NUM: bad number %i", n);
    }
    return (edict_t*)((byte*)qcvm->edicts + (n)*qcvm->edict_size);
}

int NUM_FOR_EDICT(edict_t* e)
{
    int b;

    b = (byte*)e - (byte*)qcvm->edicts;
    b = b / qcvm->edict_size;

    if(b < 0 || b >= qcvm->num_edicts)
    {
        Host_Error("NUM_FOR_EDICT: bad pointer");
    }
    return b;
}

//===========================================================================


#define PR_STRING_ALLOCSLOTS 256

static void PR_AllocStringSlots()
{
    qcvm->maxknownstrings += PR_STRING_ALLOCSLOTS;
    Con_DPrintf2(
        "PR_AllocStringSlots: realloc'ing for %d slots\n", qcvm->maxknownstrings);
    qcvm->knownstrings = (const char**)Z_Realloc(
        (void*)qcvm->knownstrings, qcvm->maxknownstrings * sizeof(char*));
}

const char* PR_GetString(int num)
{
    if(num >= 0 && num < qcvm->stringssize)
    {
        return qcvm->strings + num;
    }

    if(num < 0 && num >= -qcvm->numknownstrings)
    {
        if(!qcvm->knownstrings[-1 - num])
        {
            Host_Error(
                "PR_GetString: attempt to get a non-existant string %d\n", num);

            return "";
        }

        return qcvm->knownstrings[-1 - num];
    }
    else
    {
        Host_Error("PR_GetString: invalid string offset %d\n", num);
        return "";
    }
}

void PR_ClearEngineString(int num)
{
    if(num < 0 && num >= -qcvm->numknownstrings)
    {
        num = -1 - num;
        qcvm->knownstrings[num] = nullptr;
        if(qcvm->freeknownstrings > num)
        {
            qcvm->freeknownstrings = num;
        }
    }
}

int PR_SetEngineString(const char* s)
{
    int i;

    if(!s)
    {
        return 0;
    }
#if 0 /* can't: sv.model_precache & sv.sound_precache points to qcvm->strings */
	if (s >= qcvm->strings && s <= qcvm->strings + qcvm->stringssize)
		Host_Error("PR_SetEngineString: \"%s\" in qcvm->strings area\n", s);
#else
    if(s >= qcvm->strings && s <= qcvm->strings + qcvm->stringssize - 2)
    {
        return (int)(s - qcvm->strings);
    }
#endif
    for(i = 0; i < qcvm->numknownstrings; i++)
    {
        if(qcvm->knownstrings[i] == s)
        {
            return -1 - i;
        }
    }
    // new unknown engine string
    // Con_DPrintf ("PR_SetEngineString: new engine string %p\n", s);
#if 0
	for (i = 0; i < qcvm->numknownstrings; i++)
	{
		if (!qcvm->knownstrings[i])
			break;
	}
#endif
    //	if (i >= qcvm->numknownstrings)
    //	{
    if(i >= qcvm->maxknownstrings)
    {
        PR_AllocStringSlots();
    }
    qcvm->numknownstrings++;
    //	}
    qcvm->knownstrings[i] = s;
    return -1 - i;
}

int PR_AllocString(int size, char** ptr)
{
    int i;

    if(!size)
    {
        return 0;
    }
    for(i = 0; i < qcvm->numknownstrings; i++)
    {
        if(!qcvm->knownstrings[i])
        {
            break;
        }
    }
    //	if (i >= qcvm->numknownstrings)
    //	{
    if(i >= qcvm->maxknownstrings)
    {
        PR_AllocStringSlots();
    }
    qcvm->numknownstrings++;
    //	}
    qcvm->knownstrings[i] = (char*)Hunk_AllocName(size, "string");
    if(ptr)
    {
        *ptr = (char*)qcvm->knownstrings[i];
    }
    return -1 - i;
}
