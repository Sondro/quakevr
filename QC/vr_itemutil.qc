//
//
//
// ----------------------------------------------------------------------------
// Implementation utilities
// ----------------------------------------------------------------------------

void(string xFnName, float xItemId) VR_ItemUtil_Impl_PrintInvalidItemId =
{
    bprint("`");
    bprint(xFnName);
    bprint("`: invalid item id '");
    bprint(ftos(xItemId));
    bprint("'\n");
}

void(string xFnName, float xItemId) VR_ItemUtil_Impl_PrintInvalidItemCategory =
{
    bprint("`");
    bprint(xFnName);
    bprint("`: invalid item category for item '");
    bprint(ftos(xItemId));
    bprint("'\n");
}

float(float xItemId) VR_ItemUtil_Impl_ItemIdToFlag =
{
    // `IID_CATEGORY_ITEMS`
    if(xItemId == IID_FIST)               { return IT_FIST; }
    if(xItemId == IID_SHOTGUN)            { return IT_SHOTGUN; }
    if(xItemId == IID_SUPER_SHOTGUN)      { return IT_SUPER_SHOTGUN; }
    if(xItemId == IID_NAILGUN)            { return IT_NAILGUN; }
    if(xItemId == IID_SUPER_NAILGUN)      { return IT_SUPER_NAILGUN; }
    if(xItemId == IID_GRENADE_LAUNCHER)   { return IT_GRENADE_LAUNCHER; }
    if(xItemId == IID_ROCKET_LAUNCHER)    { return IT_ROCKET_LAUNCHER; }
    if(xItemId == IID_LIGHTNING)          { return IT_LIGHTNING; }
    if(xItemId == IID_MJOLNIR)            { return IT_MJOLNIR; }
    if(xItemId == IID_SHELLS)             { return IT_SHELLS; }
    if(xItemId == IID_NAILS)              { return IT_NAILS; }
    if(xItemId == IID_ROCKETS)            { return IT_ROCKETS; }
    if(xItemId == IID_CELLS)              { return IT_CELLS; }
    if(xItemId == IID_AXE)                { return IT_AXE; }
    if(xItemId == IID_ARMOR1)             { return IT_ARMOR1; }
    if(xItemId == IID_ARMOR2)             { return IT_ARMOR2; }
    if(xItemId == IID_ARMOR3)             { return IT_ARMOR3; }
    if(xItemId == IID_SUPERHEALTH)        { return IT_SUPERHEALTH; }
    if(xItemId == IID_KEY1)               { return IT_KEY1; }
    if(xItemId == IID_KEY2)               { return IT_KEY2; }
    if(xItemId == IID_INVISIBILITY)       { return IT_INVISIBILITY; }
    if(xItemId == IID_INVULNERABILITY)    { return IT_INVULNERABILITY; }
    if(xItemId == IID_SUIT)               { return IT_SUIT; }
    if(xItemId == IID_QUAD)               { return IT_QUAD; }
    if(xItemId == IID_LASER_CANNON)       { return IT_LASER_CANNON; }

    // `IID_CATEGORY_HIPNOTIC_ITEMS`
    if(xItemId == IID_WETSUIT)            { return HIP_IT_WETSUIT; }
    if(xItemId == IID_EMPATHY_SHIELDS)    { return HIP_IT_EMPATHY_SHIELDS; }
    if(xItemId == IID_HORN_OF_CONJURING)  { return HIP_IT_HORN_OF_CONJURING; }
    if(xItemId == IID_PROXIMITY_GUN)      { return HIP_IT_PROXIMITY_GUN; }

    // `IID_CATEGORY_ROGUE_ITEMS`
    if(xItemId == IID_LAVA_NAILGUN)       { return ROGUE_IT_LAVA_NAILGUN; }
    if(xItemId == IID_LAVA_SUPER_NAILGUN) { return ROGUE_IT_LAVA_SUPER_NAILGUN; }
    if(xItemId == IID_MULTI_GRENADE)      { return ROGUE_IT_MULTI_GRENADE; }
    if(xItemId == IID_MULTI_ROCKET)       { return ROGUE_IT_MULTI_ROCKET; }
    if(xItemId == IID_PLASMA_GUN)         { return ROGUE_IT_PLASMA_GUN; }
    if(xItemId == IID_GRAPPLE)            { return ROGUE_IT_GRAPPLE; }
    if(xItemId == IID_LAVA_NAILS)         { return ROGUE_IT_LAVA_NAILS; }
    if(xItemId == IID_PLASMA_AMMO)        { return ROGUE_IT_PLASMA_AMMO; }
    if(xItemId == IID_MULTI_ROCKETS)      { return ROGUE_IT_MULTI_ROCKETS; }
    if(xItemId == IID_SHIELD)             { return ROGUE_IT_SHIELD; }
    if(xItemId == IID_ANTIGRAV)           { return ROGUE_IT_ANTIGRAV; }
    if(xItemId == IID_EARTHQUAKE)         { return ROGUE_IT_EARTHQUAKE; }
    if(xItemId == IID_V_SPHERE)           { return ROGUE_IT_V_SPHERE; }

    VR_ItemUtil_Impl_PrintInvalidItemId("VR_ItemUtil_Impl_ItemIdToFlag",
                                        xItemId);

    return 0;
}

//
//
//
// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

float(float xItemId) VR_ItemUtil_IsValidItemId =
{
    return (xItemId >= IID_ARMOR1)
        && (xItemId <= IID_V_SPHERE);
}

float(float xItemId) VR_ItemUtil_ItemIdToItemCategory =
{
    // `IID_CATEGORY_ITEMS`
    if(xItemId == IID_FIST)              { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_SHOTGUN)           { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_SUPER_SHOTGUN)     { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_NAILGUN)           { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_SUPER_NAILGUN)     { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_GRENADE_LAUNCHER)  { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_ROCKET_LAUNCHER)   { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_LIGHTNING)         { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_MJOLNIR)           { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_SHELLS)            { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_NAILS)             { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_ROCKETS)           { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_CELLS)             { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_AXE)               { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_ARMOR1)            { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_ARMOR2)            { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_ARMOR3)            { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_SUPERHEALTH)       { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_KEY1)              { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_KEY2)              { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_INVISIBILITY)      { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_INVULNERABILITY)   { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_SUIT)              { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_QUAD)              { return IID_CATEGORY_ITEMS; }
    if(xItemId == IID_LASER_CANNON)      { return IID_CATEGORY_ITEMS; }

    // `IID_CATEGORY_HIPNOTIC_ITEMS`
    if(xItemId == IID_WETSUIT)           { return IID_CATEGORY_HIPNOTIC_ITEMS; }
    if(xItemId == IID_EMPATHY_SHIELDS)   { return IID_CATEGORY_HIPNOTIC_ITEMS; }
    if(xItemId == IID_HORN_OF_CONJURING) { return IID_CATEGORY_HIPNOTIC_ITEMS; }
    if(xItemId == IID_PROXIMITY_GUN)     { return IID_CATEGORY_HIPNOTIC_ITEMS; }

    // `IID_CATEGORY_ROGUE_ITEMS`
    if(xItemId == IID_LAVA_NAILGUN)       { return IID_CATEGORY_ROGUE_ITEMS; }
    if(xItemId == IID_LAVA_SUPER_NAILGUN) { return IID_CATEGORY_ROGUE_ITEMS; }
    if(xItemId == IID_MULTI_GRENADE)      { return IID_CATEGORY_ROGUE_ITEMS; }
    if(xItemId == IID_MULTI_ROCKET)       { return IID_CATEGORY_ROGUE_ITEMS; }
    if(xItemId == IID_PLASMA_GUN)         { return IID_CATEGORY_ROGUE_ITEMS; }
    if(xItemId == IID_GRAPPLE)            { return IID_CATEGORY_ROGUE_ITEMS; }
    if(xItemId == IID_LAVA_NAILS)         { return IID_CATEGORY_ROGUE_ITEMS; }
    if(xItemId == IID_PLASMA_AMMO)        { return IID_CATEGORY_ROGUE_ITEMS; }
    if(xItemId == IID_MULTI_ROCKETS)      { return IID_CATEGORY_ROGUE_ITEMS; }
    if(xItemId == IID_SHIELD)             { return IID_CATEGORY_ROGUE_ITEMS; }
    if(xItemId == IID_ANTIGRAV)           { return IID_CATEGORY_ROGUE_ITEMS; }
    if(xItemId == IID_EARTHQUAKE)         { return IID_CATEGORY_ROGUE_ITEMS; }
    if(xItemId == IID_V_SPHERE)           { return IID_CATEGORY_ROGUE_ITEMS; }

    VR_ItemUtil_Impl_PrintInvalidItemId("VR_ItemUtil_ItemIdToItemCategory",
                                        xItemId);

    return IID_CATEGORY_INVALID;
}

float(entity xEnt, float xItemId) VR_ItemUtil_EntHasItem =
{
    float itemCategory, itemFlag;
    itemCategory = VR_ItemUtil_ItemIdToItemCategory(xItemId);
    itemFlag = VR_ItemUtil_Impl_ItemIdToFlag(xItemId);


    if(itemCategory == IID_CATEGORY_ITEMS)
    {
        return VRHasBit(xEnt.items, itemFlag);
    }

    if(itemCategory == IID_CATEGORY_HIPNOTIC_ITEMS)
    {
        return VRHasBit(xEnt.hipnotic_items, itemFlag);
    }

    if(itemCategory == IID_CATEGORY_ROGUE_ITEMS)
    {
        return VRHasBit(xEnt.rogue_items, itemFlag);
    }

    VR_ItemUtil_Impl_PrintInvalidItemCategory("VR_ItemUtil_EntHasItem",
                                              xItemId);

    return FALSE;
}

void(entity xEnt, float xItemId, float xOwned) VR_ItemUtil_EntSetItem =
{
    float itemCategory, itemFlag;
    itemCategory = VR_ItemUtil_ItemIdToItemCategory(xItemId);
    itemFlag = VR_ItemUtil_Impl_ItemIdToFlag(xItemId);

    if(itemCategory == IID_CATEGORY_ITEMS)
    {
        xEnt.items = VRSetBit(xEnt.items, itemFlag, xOwned);
        return;
    }

    if(itemCategory == IID_CATEGORY_HIPNOTIC_ITEMS)
    {
        xEnt.hipnotic_items = VRSetBit(xEnt.hipnotic_items, itemFlag, xOwned);
        return;
    }

    if(itemCategory == IID_CATEGORY_ROGUE_ITEMS)
    {
        xEnt.rogue_items = VRSetBit(xEnt.rogue_items, itemFlag, xOwned);
        return;
    }

    VR_ItemUtil_Impl_PrintInvalidItemCategory("VR_ItemUtil_EntSetItem",
                                              xItemId);
}

void(entity xEnt, float xItemId) VR_ItemUtil_EntAddItem =
{
    VR_ItemUtil_EntSetItem(xEnt, xItemId, TRUE);
}

void(entity xEnt, float xItemId) VR_ItemUtil_EntDelItem =
{
    VR_ItemUtil_EntSetItem(xEnt, xItemId, FALSE);
}

void(entity xEnt) VR_ItemUtil_EntClearItems =
{
    xEnt.items = 0;
    xEnt.hipnotic_items = 0;
    xEnt.rogue_items = 0;
}
