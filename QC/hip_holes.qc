/* Bullet holes QuickC program
   By Jim Dose'  11/20/96
   Copyright (c)1996 Hipnotic Interactive, Inc.
   All rights reserved.
   Do not distribute.
*/

/*QUAKED wallsprite (0 1 0) (-8 -8 -8) (8 8 8)
Places a sprite on a wall.  Angles should be opposite of face.

"model" sprite to place on wall.  Default is "progs/s_blood1.spr".
*/
void() wallsprite =
   {
   if(!self.model)
      {
      self.model = "progs/s_blood1.spr";
      }

   precache_model(self.model);
   setmodel(self, self.model);

   // QuakeEd doesn't save up and down angles properly.
   if(self.angles == '0 -1 0')
      self.angles = '-90 0 0';
    else if(self.angles == '0 -2 0')
      self.angles = '90 0 0';

   // Pull the sprite away from the wall slightly to
   // get rid of z sort errors.
   makeforward(self.angles);
   setorigin(self, self.origin - (v_forward * 0.2));
    makestatic(self);
   };

void() InitBulletHoles =
   {
   precache_model("progs/s_bullet.spr");

   bulletholes = nullentity;
   lastbullet = nullentity;
   numbulletholes = 0;
   lastbulletholeroll = 0;
   };

void() remove_bullethole =
   {
   entity ent;

   // There is a possibility that this is not the first bullet
   // in the list, but it doesn't really matter.  All that
   // matters is there is one less bullet.  Just make sure
   // we don't remove the world!
   if(bulletholes == nullentity)
      {
      objerror("remove_bullethole: bulletholes == nullentity! ");
      }

   ent = bulletholes;
   if(ent.classname != "bullethole")
      {
      objerror("remove_bullethole: Tried to remove non-bullethole!");
      }

   bulletholes = bulletholes.lastvictim;
   remove(ent);
   if(lastbullet == ent)
      {
      lastbullet = nullentity;
      }
   numbulletholes = numbulletholes - 1;
   };

void(vector pos) placebullethole =
{
    entity new;
    vector norm;

    new = spawn();
    new.owner = new;
    new.movetype = MOVETYPE_NONE;
    new.solid = SOLID_NOT;
    new.classname = "bullethole";
    setmodel(new, "progs/s_bullet.spr");
    setsize(new, '0 0 0', '0 0 0');

    norm = trace_plane_normal;
    norm_x = 0 - norm_x;
    norm_y = 0 - norm_y;
    new.angles = vectoangles(norm);
    new.angles_z = lastbulletholeroll;

    lastbulletholeroll = lastbulletholeroll + 35 + 15 * random();

    if(lastbulletholeroll > 360)
    {
       lastbulletholeroll = 0;
    }

    makeforward_mdl(self.angles);
    setorigin(new, pos - (v_forward * (0.03 + new.angles_z / 450)));
        // try to avoid z-fighting for overlapping sprites

    new.think = remove_bullethole;
    new.nextthink = time + 250 + (150 * random());

    numbulletholes = numbulletholes + 1;
    if(numbulletholes > 64)
    {
        remove_bullethole();
    }

    if(lastbullet != nullentity)
    {
        lastbullet.lastvictim = new;
    }
    else
    {
        bulletholes = new;
    }

    new.lastvictim = nullentity;
    lastbullet = new;
};
