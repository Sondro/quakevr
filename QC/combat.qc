// combat.qc

float IsExplosionDamage;

//============================================================================

/*
============
CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
float(entity targ, entity inflictor) CanDamage =
{
// bmodels need special checking because their origin is 0,0,0
    if(targ.movetype == MOVETYPE_PUSH)
    {
        traceline(inflictor.origin, 0.5 * (targ.absmin + targ.absmax), TRUE, self);
        if(trace_fraction == 1)
            return TRUE;
        if(trace_ent == targ)
            return TRUE;
        return FALSE;
    }

    traceline(inflictor.origin, targ.origin, TRUE, self);
    if(trace_fraction == 1)
        return TRUE;
    traceline(inflictor.origin, targ.origin + '15 15 0', TRUE, self);
    if(trace_fraction == 1)
        return TRUE;
    traceline(inflictor.origin, targ.origin + '-15 -15 0', TRUE, self);
    if(trace_fraction == 1)
        return TRUE;
    traceline(inflictor.origin, targ.origin + '-15 15 0', TRUE, self);
    if(trace_fraction == 1)
        return TRUE;
    traceline(inflictor.origin, targ.origin + '15 -15 0', TRUE, self);
    if(trace_fraction == 1)
        return TRUE;

    return FALSE;
};


/*
============
Killed
============
*/
void(entity targ, entity attacker) Killed =
{
    entity oself;

    oself = self;
    self = targ;

    if(self.health < -99)
    {
        self.health = -99;        // don't let sbar look bad if a player
    }

//MED
    if(self.charmed)
    {
        self.effects = self.effects - (self.effects & EF_DIMLIGHT);
    }

    if(self.movetype == MOVETYPE_PUSH || self.movetype == MOVETYPE_NONE)
    {    // doors, triggers, etc
        self.th_die();
        self = oself;
        return;
    }

    self.enemy = attacker;

// bump the monster counter
    if(self.flags & FL_MONSTER)
    {
        killed_monsters = killed_monsters + 1;
        WriteByte(MSG_ALL, SVC_KILLEDMONSTER);
    }

// PGM - guarantee gib if killed by buzzsaw.
    if(attacker.classname == "buzzsaw" ||
        attacker.classname == "Vengeance" ||
        attacker.classname == "pendulum")
        self.health = -99;

// PGM - gravity fix!
    self.gravity = 1.0;
    ClientObituary(self, attacker);

    self.takedamage = DAMAGE_NO;
    self.touch = SUB_Null;

    monster_death_use();
    self.th_die();

    self = oself;
};

float() getVRMeleeBloodlust =
{
    return cvar_hget(cvarh_vr_melee_bloodlust);
}

/*
============
T_Damage

The damage is coming from inflictor, but get mad at attacker
This should be the only function that ever reduces health.
============
*/
//MED 01/10/97 added empathyused variable
float empathyused;
float(entity targ, entity inflictor, entity attacker, float damage, float isVRMelee) T_DamageImpl =
{
    vector    dir;
    entity    oldself;
    float    save;
    float    take;

    if(!targ.takedamage)
    {
        return FALSE;
    }

    if(targ.classname == "trigger_explosion")
    {
        if(IsExplosionDamage != 1)
        {
            return FALSE;
        }
    }

    IsExplosionDamage = 0;

    if(VR_ItemUtil_EntHasItem(targ, IID_SHIELD))
    {
        // Allow some damage to pass through.
        damage = shield_hit(targ, inflictor, attacker, damage);

        if(damage == 0)
        {
            return FALSE;
        }
    }

    if(discharged && targ.wetsuit_finished)
    {
        return FALSE;
    }

    //MED
    // moved damage_attacker down a bit

    // HONEY
    // cg: check if this needs a specific damage type
    if((targ.flags & FL_SPECIFICDAMAGE) && !(attacker.damage_weapon & targ.items)) {
        return FALSE;
    }

    // check for quad damage powerup on the attacker
    if(attacker.super_damage_finished > time)
    {
        damage = damage * 4;
    }

    // rune power up (Black Magic)
    if(deathmatch)
    {
        damage = RuneApplyBlack(damage, attacker);
    }

    //MED
    //check for empathy shields
    if(VR_ItemUtil_EntHasItem(targ, IID_EMPATHY_SHIELDS) &&
       !VR_ItemUtil_EntHasItem(inflictor, IID_EMPATHY_SHIELDS) &&
       (targ != attacker))
    {
        empathyused = 1;
        damage = damage/2;
        T_DamageImpl (attacker,targ,targ,damage,isVRMelee);
        empathyused = 0;
    }

    //MED
    // used by buttons and triggers to set activator for target firing
    damage_attacker = attacker;

    //MED
    // used to keep track of what hit us
    damage_inflictor = inflictor;

// save damage based on the target's armor level

    //ZOID--
    if(TeamArmorDam(targ, inflictor, attacker, damage))
    {
        save = ceil(targ.armortype * damage);
    }
    else
    {
        save = 0;
    }
    //--ZOID

    if(save >= targ.armorvalue)
    {
        save = targ.armorvalue;
        targ.armortype = 0;    // lost all armor

        VR_ItemUtil_EntDelItem(targ, IID_ARMOR1);
        VR_ItemUtil_EntDelItem(targ, IID_ARMOR2);
        VR_ItemUtil_EntDelItem(targ, IID_ARMOR3);
    }

    targ.armorvalue = targ.armorvalue - save;
    take = ceil(damage-save);

    // add to the damage total for clients, which will be sent as a single
    // message at the end of the frame
    // FIXME: remove after combining shotgun blasts?
    if(targ.flags & FL_CLIENT)
    {
        targ.dmg_take = targ.dmg_take + take;
        targ.dmg_save = targ.dmg_save + save;
        targ.dmg_inflictor = inflictor;
    }

    // figure momentum add
    if((inflictor != world) && (targ.movetype == MOVETYPE_WALK))
    {
        dir = targ.origin - (inflictor.absmin + inflictor.absmax) * 0.5;
        dir = normalize(dir);
        targ.velocity = targ.velocity + dir*damage*8;
    }

    // check for godmode or invincibility
    if(targ.flags & FL_GODMODE)
    {
        return FALSE;
    }

    if(targ.invincible_finished >= time)
    {
        if(targ.invincible_sound < time)
        {
            sound(targ, CHAN_ITEM, "items/protect3.wav", 1, ATTN_NORM);
            targ.invincible_sound = time + 2;
        }

        return TRUE;
    }

    //MED
    if(VR_ItemUtil_EntHasItem(targ, IID_EMPATHY_SHIELDS))
    {
        if(self.empathy_sound < time)
        {
            sound(targ, CHAN_ITEM, "hipitems/empathy2.wav", 1, ATTN_NORM);
            self.empathy_sound = time + 0.5;
        }
    }

    // team play damage avoidance
    //ZOID--
    if(!TeamHealthDam(targ, inflictor, attacker, damage))
    {
        return TRUE; // same team
    }
    //--ZOID

    if(targ.classname == "player")
    {
        SpawnBloodSplash(targ.origin, take / 2.0);
        SpawnBloodSplash(targ.handpos, take / 2.0);
        SpawnBloodSplash(targ.offhandpos, take / 2.0);

        if(targ.ishuman)
        {
            haptic(cVR_OffHand, 0.0, 0.5, 30, 1.0);
            haptic(cVR_MainHand, 0.0, 0.5, 30, 1.0);
        }
    }

    // rune power up (Earth Magic)
    if(deathmatch)
    {
        take = RuneApplyEarth(take, targ);
    }

    // do the damage
    targ.health = targ.health - take;

    if(targ.health <= 0)
    {
        if(isVRMelee)
        {
            // VR melee takedown
            if(getVRMeleeBloodlust() == QVR_MELEEBLOODLUST_ENABLED)
            {
                float bloodlustAmount = ceil(1 + take / 12) *
                    cvar_hget(cvarh_vr_melee_bloodlust_mult) * 0.75;

                T_Heal(attacker, bloodlustAmount, 1);
            }
        }

        Killed (targ, attacker);
        return TRUE;
    }
    else
    {
        if(isVRMelee)
        {
            // VR melee damage
            if(getVRMeleeBloodlust() == QVR_MELEEBLOODLUST_ENABLED)
            {
                T_Heal(attacker, ceil(take / 12), 1);
            }
        }
    }

    // react to the damage
    oldself = self;
    self = targ;

    //MED 10/17/96 added charmed stuff
    if((self.flags & FL_MONSTER) && attacker != world)
    {
        // get mad unless of the same class (except for soldiers)
        if(self != attacker && attacker != self.enemy && (self.charmer != attacker))
        {
            if((self.classname != attacker.classname)
            || (self.classname == "monster_army") || (self.classname == "monster_armagon"))
            {
                if(self.classname != "monster_dragon")
                {
                    if(self.enemy.classname == "player")
                    {
                        self.oldenemy = self.enemy;
                    }

                    self.enemy = attacker;
                    FoundTarget();
                }
            }
        }
    }

    if(self.th_pain)
    {
        self.th_pain (attacker, take);
    // nightmare mode monsters don't go into pain frames often
        if(skill == 3)
            self.pain_finished = time + 5;
    }

    self = oldself;
    return TRUE;
};

float(entity targ, entity inflictor, entity attacker, float damage) T_Damage =
{
    return T_DamageImpl(targ, inflictor, attacker, damage, 0);
};

float(entity targ, entity inflictor, entity attacker, float damage) T_Damage_VRMelee =
{
    return T_DamageImpl(targ, inflictor, attacker, damage, 1);
};

/*
============
T_RadiusDamage
============
*/
void(entity inflictor, entity attacker, float damage, entity ignore) T_RadiusDamage =
{
    float     points;
    entity    head;
    vector    org;

    IsExplosionDamage = 1;

    head = findradius(inflictor.origin, damage+40);

    while(head)
    {
        if(head != ignore)
        {
            if(head.takedamage)
            {
                org = head.origin + (head.mins + head.maxs)*0.5;
                points = 0.5*vlen(inflictor.origin - org);
                if(points < 0)
                    points = 0;
                points = damage - points;
                if(head == attacker)
                    points = points * 0.5;
                if(points > 0)
                {
                    if(CanDamage(head, inflictor))
                    {    // shambler takes half damage from all explosions
                        if(head.classname == "monster_shambler")
                            T_Damage(head, inflictor, attacker, points*0.5);
                        else
                            T_Damage(head, inflictor, attacker, points);
                    }
                }
            }
        }
        head = head.chain;
    }
};

/*
============
T_BeamDamage
============
*/
void(entity attacker, float damage) T_BeamDamage =
{
    float     points;
    entity    head;

    head = findradius(attacker.origin, damage+40);

    while(head)
    {
        if(head.takedamage)
        {
            points = 0.5*vlen(attacker.origin - head.origin);
            if(points < 0)
                points = 0;
            points = damage - points;
            if(head == attacker)
                points = points * 0.5;
            if(points > 0)
            {
                if(CanDamage(head, attacker))
                {
                    if(head.classname == "monster_shambler")
                        T_Damage(head, attacker, attacker, points*0.5);
                    else
                        T_Damage(head, attacker, attacker, points);
                }
            }
        }
        head = head.chain;
    }
};
