#include "progs.hpp"

struct qpic_t;

struct
{
    char name[MAX_QPATH];
    int type;
    qpic_t* pic;
} * qcpics;

size_t numqcpics;
size_t maxqcpics;

void PR_ReloadPics(bool purge)
{
    numqcpics = 0;

    free(qcpics);
    qcpics = nullptr;
    maxqcpics = 0;
}

void PR_AutoCvarChanged(cvar_t* var)
{
    (void)var;

    // TODO VR: (P0): QSS Merge
#if 0
    char* n;
    ddef_t* glob;
    qcvm_t* oldqcvm = qcvm;
    PR_SwitchQCVM(nullptr);
    if(sv.active)
    {
        PR_SwitchQCVM(&sv.qcvm);
        n = va("autocvar_%s", var->name);
        glob = ED_FindGlobal(n);
        if(glob)
        {
            if(!ED_ParseEpair((void*)qcvm->globals, glob, var->string))
            {
                Con_Warning("EXT: Unable to configure %s\n", n);
            }
        }
        PR_SwitchQCVM(nullptr);
    }
    if(cl.qcvm.globals)
    {
        PR_SwitchQCVM(nullptr);
        PR_SwitchQCVM(&cl.qcvm);
        n = va("autocvar_%s", var->name);
        glob = ED_FindGlobal(n);
        if(glob)
        {
            if(!ED_ParseEpair((void*)qcvm->globals, glob, var->string))
            {
                Con_Warning("EXT: Unable to configure %s\n", n);
            }
        }
        PR_SwitchQCVM(nullptr);
    }
    PR_SwitchQCVM(oldqcvm);
#endif
}
