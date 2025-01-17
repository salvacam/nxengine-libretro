
// sprites routines
#include "graphics.h"
#include <string.h>
#include "../siflib/sif.h"
#include "../siflib/sifloader.h"
#include "../siflib/sectSprites.h"
#include "../siflib/sectStringArray.h"
#include "../autogen/sprites.h"
#include "../common/StringList.h"
#include "../dirnames.h"
#include "../settings.h"
#include "../nx.h"
using namespace Graphics;

#include "sprites.h"
#include "sprites.fdh"

#include "../libretro/libretro_shared.h"

static NXSurface *spritesheet[MAX_SPRITESHEETS];
static int num_spritesheets;
static StringList sheetfiles;

SIFSprite sprites[MAX_SPRITES];
int num_sprites;

bool Sprites::Init()
{
   char f_sprites_sif[1024];
   memset(spritesheet, 0, sizeof(spritesheet));

   retro_create_path_string(f_sprites_sif, sizeof(f_sprites_sif), "data", "sprites.sif");

   // load sprites info--sheet positions, bounding boxes etc
   if (load_sif(f_sprites_sif))
      return 1;

   num_spritesheets = sheetfiles.CountItems();
   return 0;
}

void Sprites::Close()
{
   FlushSheets();
   sheetfiles.MakeEmpty();
}

void Sprites::FlushSheets()
{
   for(int i=0;i<MAX_SPRITESHEETS;i++)
   {
      if (spritesheet[i])
      {
         delete spritesheet[i];
         spritesheet[i] = NULL;
      }
   }
}

/*
   void c------------------------------() {}
 */

// ensure the given spritesheet is loaded
static void Sprites_LoadSheetIfNeeded(int sheetno)
{
   if (!spritesheet[sheetno])
   {
      char pbm_name[1024];
      retro_create_path_string(pbm_name, sizeof(pbm_name), data_dir, sheetfiles.StringAt(sheetno));

#ifdef _WIN32
      for (unsigned i = 0; i < sizeof(pbm_name); i++)
      {
         if (pbm_name[i] == '/')
            pbm_name[i] = '\\';
      }
#endif

      spritesheet[sheetno] = new NXSurface;
      spritesheet[sheetno]->LoadImage(pbm_name, true);

      // fix the blue dash in the middle of the starpoof effect on that one frame,
      // I'm pretty sure this is a glitch.
      if (!settings->emulate_bugs)
      {
         if (sheetno == 3)	// Caret.pbm
            spritesheet[sheetno]->FillRect(40, 58, 41, 58, 0, 0, 0);
      }
   }
}


// master sprite drawing function
static void Sprites_BlitSprite(int x, int y, int s, int frame, uint8_t dir,
      int xoff, int yoff, int wd, int ht)
{
   Sprites_LoadSheetIfNeeded(sprites[s].spritesheet);

   dir %= sprites[s].ndirs;
   SIFDir *sprdir = &sprites[s].frame[frame].dir[dir];

   DrawSurface(spritesheet[sprites[s].spritesheet], 
         x, y,
         (sprdir->sheet_offset.x + xoff), 
         (sprdir->sheet_offset.y + yoff), 
         wd, ht);
}

/*
   void c------------------------------() {}
 */


// draw sprite "s" at [x,y]. drawing frame "frame" and dir "dir".
void Sprites::draw_sprite(int x, int y, int s, int frame, uint8_t dir)
{
   Sprites_BlitSprite(x, y, s, frame, dir, 0, 0, sprites[s].w, sprites[s].h);
}

// draw sprite "s", place it's draw point at [x,y] instead of it's upper-left corner.
void Sprites::draw_sprite_at_dp(int x, int y, int s, int frame, uint8_t dir)
{
   x -= sprites[s].frame[frame].dir[dir].drawpoint.x;
   y -= sprites[s].frame[frame].dir[dir].drawpoint.y;
   Sprites_BlitSprite(x, y, s, frame, dir, 0, 0, sprites[s].w, sprites[s].h);
}


// draw a portion of a sprite, such as a sprite in the middle of "teleporting".
// only the area between clipy1 (inclusive) and clipy2 (exclusive) are visible.
void Sprites::draw_sprite_clipped(int x, int y, int s, int frame, uint8_t dir, \
      int clipx1, int clipx2, int clipy1, int clipy2)
{
   Sprites_BlitSprite(x + clipx1, y + clipy1, s, frame, dir, clipx1, clipy1, 
         (clipx2 - clipx1), (clipy2 - clipy1));
}

// draw a clipped sprite while clipping only the width.
// used for drawing percentage bars, etc.
void Sprites::draw_sprite_clip_width(int x, int y, int s, int frame, int wd)
{
   Sprites_BlitSprite(x, y, s, frame, 0, 0, 0, wd, sprites[s].h);
}

// draws a sprite at less than it's actual width by chopping it into two chunks.
// on the left, the first "repeat_at" pixels are drawn.
// then, the remaining "wd" is drawn from the right half of the sprite.
// used for things like drawing the textboxes.
void Sprites::draw_sprite_chopped(int x, int y, int s, int frame, int wd, int repeat_at)
{
   int xoff;

   if (wd >= sprites[s].w)
   {
      draw_sprite(x, y, s, frame);
      return;
   }

   // draw the left part
   Sprites_BlitSprite(x, y, s, frame, 0, 0, 0, repeat_at, sprites[s].h);
   x += repeat_at;
   wd -= repeat_at;

   // draw the rest of it
   xoff = (sprites[s].w - wd);

   Sprites_BlitSprite(x, y, s, frame, 0, xoff, 0, wd, sprites[s].h);
}

// draws a sprite to any arbitrary width by repeating it over the given distance.
// if needed, the rightmost instance of the sprite is clipped.
void Sprites::draw_sprite_repeating_x(int x, int y, int s, int frame, int wd)
{
   int wdleft = wd;
   while(wdleft > 0)
   {
      int blitwd = wdleft;
      if (blitwd > sprites[s].w) blitwd = sprites[s].w;

      Sprites_BlitSprite(x, y, s, frame, 0, 0, 0, blitwd, sprites[s].h);
      x += blitwd;
      wdleft -= blitwd;
   }
}

/*
   void c------------------------------() {}
 */

// return the NXSurface for a given spritesheet #
NXSurface *Sprites::get_spritesheet(int sheetno)
{
   Sprites_LoadSheetIfNeeded(sheetno);
   return spritesheet[sheetno];
}

// create an empty spritesheet of the given size and return it's index.
int Sprites::create_spritesheet(int wd, int ht)
{
   if (num_spritesheets >= MAX_SPRITESHEETS)
      return -1;

   spritesheet[num_spritesheets] = new NXSurface(wd, ht);
   return num_spritesheets++;
}

// draw a sprite onto some surface other than the screen
void Sprites::draw_sprite_to_surface(NXSurface *dst, int x, int y, int s, int frame, uint8_t dir)
{
   Graphics::SetDrawTarget(dst);
   draw_sprite(x, y, s, frame, dir);
   Graphics::SetDrawTarget(screen);
}

/*
   void c------------------------------() {}
 */

static bool load_sif(const char *fname)
{
   SIFLoader sif;
   uint8_t *sheetdata, *spritesdata;
   int sheetdatalength, spritesdatalength;

   if (sif.LoadHeader(fname))
      return 1;

   if (!(sheetdata = sif.FindSection(SIF_SECTION_SHEETS, &sheetdatalength)))
      return 1;

   if (!(spritesdata = sif.FindSection(SIF_SECTION_SPRITES, &spritesdatalength)))
      return 1;

   // decode sheets
   sheetfiles.MakeEmpty();
   if (SIFStringArraySect::Decode(sheetdata, sheetdatalength, &sheetfiles))
      return 1;

   // decode sprites
   if (SIFSpritesSect::Decode(spritesdata, spritesdatalength,
            &sprites[0], &num_sprites, MAX_SPRITES))
      return 1;

   sif.CloseFile();

   create_slope_boxes();
   offset_by_draw_points();

   // for sprites which only have 1 dir 
   // (no separate frames for left & right),
   // create a 2nd identical dir as the rest 
   // of the engine doesn't bother
   // with this complication.

   for(int s=0;s<num_sprites;s++)
   {
      if (sprites[s].ndirs == 1)
      {
         sprites[s].ndirs = 2;
         for(int f=0;f<sprites[s].nframes;f++)
            sprites[s].frame[f].dir[1] = sprites[s].frame[f].dir[0];
      }
   }

   return 0;
}


// create slope boxes for all sprites, used by the slope-handling routines
// these are basically just a form of bounding box describing the bounds of the
// blockd points.
static void create_slope_boxes()
{
   for(int s=0;s<num_sprites;s++)
   {
      if (sprites[s].block_d.count != 0)
      {
         int leftmost = 99999;
         int rightmost = -99999;
         for(int i=0;i<sprites[s].block_d.count;i++)
         {
            if (sprites[s].block_d[i].x < leftmost)  leftmost = sprites[s].block_d[i].x;
            if (sprites[s].block_d[i].x > rightmost) rightmost = sprites[s].block_d[i].x;
         }

         sprites[s].slopebox.x1 = leftmost;
         sprites[s].slopebox.x2 = rightmost;

         if (sprites[s].block_u.count)
            sprites[s].slopebox.y1 = (sprites[s].block_u[0].y + 1);
         else
            sprites[s].slopebox.y1 = 0;

         sprites[s].slopebox.y2 = (sprites[s].block_d[0].y - 1);
      }
   }

   sprites[SPR_MYCHAR].slopebox.y1 += 3;
}

// offset things like blockl/r/u/d, bounding box etc by the draw point of all
// sprites so that these things are consistent with where the sprite appears to be
static void offset_by_draw_points()
{
   for(int s=0;s<num_sprites;s++)
   {
      int dx = -sprites[s].frame[0].dir[0].drawpoint.x;
      int dy = -sprites[s].frame[0].dir[0].drawpoint.y;

      sprites[s].bbox.offset(dx, dy);
      sprites[s].slopebox.offset(dx, dy);
      sprites[s].solidbox.offset(dx, dy);

      sprites[s].block_l.offset(dx, dy);
      sprites[s].block_r.offset(dx, dy);
      sprites[s].block_u.offset(dx, dy);
      sprites[s].block_d.offset(dx, dy);

      for(int f=0;f<sprites[s].nframes;f++)
      {
         for(int d=0;d<sprites[s].ndirs;d++)
         {
            int dx = -sprites[s].frame[f].dir[d].drawpoint.x;
            int dy = -sprites[s].frame[f].dir[d].drawpoint.y;
            sprites[s].frame[f].dir[d].pf_bbox.offset(dx, dy);
         }
      }
   }
}
