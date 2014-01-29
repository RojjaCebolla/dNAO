/*	SCCS Id: @(#)weapon.c	3.4	2002/11/07	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
 *	This module contains code for calculation of "to hit" and damage
 *	bonuses for any given weapon used, as well as weapons selection
 *	code for monsters.
 */
#include "hack.h"

#ifdef DUMP_LOG
STATIC_DCL int FDECL(enhance_skill, (boolean));
#endif

/* Categories whose names don't come from OBJ_NAME(objects[type])
 */
#define PN_BARE_HANDED			(-1)	/* includes martial arts */
#define PN_TWO_WEAPONS			(-2)
#define PN_RIDING			(-3)
#define PN_POLEARMS			(-4)
#define PN_SABER			(-5)
#define PN_HAMMER			(-6)
#define PN_WHIP				(-7)
#define PN_ATTACK_SPELL			(-8)
#define PN_HEALING_SPELL		(-9)
#define PN_DIVINATION_SPELL		(-10)
#define PN_ENCHANTMENT_SPELL		(-11)
#define PN_CLERIC_SPELL			(-12)
#define PN_ESCAPE_SPELL			(-13)
#define PN_MATTER_SPELL			(-14)
#define PN_HARVEST			(-15)
static void FDECL(mon_ignite_lightsaber, (struct obj *, struct monst *));

STATIC_DCL void FDECL(give_may_advance_msg, (int));

//extern struct monst zeromonst;

#ifndef OVLB

STATIC_DCL NEARDATA const short skill_names_indices[];
STATIC_DCL NEARDATA const char *odd_skill_names[];
STATIC_DCL NEARDATA const char *barehands_or_martial[];

#else	/* OVLB */

STATIC_VAR NEARDATA const short skill_names_indices[P_NUM_SKILLS] = {
	0,                DAGGER,         KNIFE,        AXE,
	PICK_AXE,         SHORT_SWORD,    BROADSWORD,   LONG_SWORD,
	TWO_HANDED_SWORD, SCIMITAR,       PN_SABER,     CLUB,
	MACE,             MORNING_STAR,   FLAIL,
	PN_HAMMER,        QUARTERSTAFF,   PN_POLEARMS,  SPEAR,
	JAVELIN,          TRIDENT,        LANCE,        BOW,
	SLING,            CROSSBOW,       DART,
	SHURIKEN,         BOOMERANG,      PN_WHIP,      PN_HARVEST,
	UNICORN_HORN,
	PN_ATTACK_SPELL,     PN_HEALING_SPELL,
	PN_DIVINATION_SPELL, PN_ENCHANTMENT_SPELL,
	PN_CLERIC_SPELL,     PN_ESCAPE_SPELL,
	PN_MATTER_SPELL,
	PN_BARE_HANDED,   PN_TWO_WEAPONS,
#ifdef STEED
	PN_RIDING
#endif
};

/* note: entry [0] isn't used */
STATIC_VAR NEARDATA const char * const odd_skill_names[] = {
    "no skill",
    "bare hands",		/* use barehands_or_martial[] instead */
    "two weapon combat",
    "riding",
    "polearms",
    "saber",
    "hammer",
    "whip",
    "attack spells",
    "healing spells",
    "divination spells",
    "enchantment spells",
    "clerical spells",
    "escape spells",
    "matter spells",
	"farm implements",
};
/* indexed vis `is_martial() */
STATIC_VAR NEARDATA const char * const barehands_or_martial[] = {
    "bare handed combat", "martial arts"
};

STATIC_OVL void
give_may_advance_msg(skill)
int skill;
{
	You_feel("more confident in your %sskills.",
		skill == P_NONE ?
			"" :
		skill <= P_LAST_WEAPON ?
			"weapon " :
		skill <= P_LAST_SPELL ?
			"spell casting " :
		"fighting ");
}

#endif	/* OVLB */

STATIC_DCL boolean FDECL(can_advance, (int, BOOLEAN_P));
STATIC_DCL boolean FDECL(could_advance, (int));
STATIC_DCL boolean FDECL(peaked_skill, (int));
STATIC_DCL int FDECL(slots_required, (int));

#ifdef OVL1

STATIC_DCL char *FDECL(skill_level_name, (int,char *));
STATIC_DCL void FDECL(skill_advance, (int));

#endif	/* OVL1 */

#define P_NAME(type) ((skill_names_indices[type] > 0) ? \
		      OBJ_NAME(objects[skill_names_indices[type]]) : \
		      (type == P_BARE_HANDED_COMBAT) ? \
			barehands_or_martial[martial_bonus()] : \
			odd_skill_names[-skill_names_indices[type]])

#ifdef OVLB
static NEARDATA const char kebabable[] = {
	S_XORN, S_DRAGON, S_JABBERWOCK, S_NAGA, S_GIANT, '\0'
};

/*
 *	hitval returns an integer representing the "to hit" bonuses
 *	of "otmp" against the monster.
 */
int
hitval(otmp, mon)
struct obj *otmp;
struct monst *mon;
{
	int	tmp = 0;
	struct permonst *ptr = mon->data;
	boolean Is_weapon = (otmp->oclass == WEAPON_CLASS || is_weptool(otmp));

	if (Is_weapon)
		tmp += otmp->spe;

/*	Put weapon specific "to hit" bonuses in below:		*/
	tmp += objects[otmp->otyp].oc_hitbon;

/*	Put weapon vs. monster type "to hit" bonuses in below:	*/

	/* Blessed weapons used against undead or demons */
	if (Is_weapon && otmp->blessed &&
	   (is_demon(ptr) || is_undead(ptr))) tmp += 2;

	if (is_spear(otmp) &&
	   index(kebabable, ptr->mlet)) tmp += 2;

	if (is_farm(otmp) &&
	    ptr->mlet == S_PLANT) tmp += 6;

	/* trident is highly effective against swimmers */
	if (otmp->otyp == TRIDENT && is_swimmer(ptr)) {
	   if (is_pool(mon->mx, mon->my)) tmp += 4;
	   else if (ptr->mlet == S_EEL || ptr->mlet == S_SNAKE) tmp += 2;
	}

	/* weapons with the veioistafur stave are highly effective against sea monsters */
	if(otmp->oclass == WEAPON_CLASS && objects[(otmp)->otyp].oc_material == WOOD && 
		(otmp->ovar1 & WARD_VEIOISTAFUR) && mon->data->mlet == S_EEL) tmp += 4;
	
	/* Picks used against xorns and earth elementals */
	if (is_pick(otmp) &&
	   (passes_walls(ptr) && thick_skinned(ptr))) tmp += 2;

#ifdef INVISIBLE_OBJECTS
	/* Invisible weapons against monsters who can't see invisible */
	if (otmp->oinvis && !perceives(ptr)) tmp += 3;
#endif

	/* Check specially named weapon "to hit" bonuses */
	if (otmp->oartifact) tmp += spec_abon(otmp, mon);

	return tmp;
}

/* Historical note: The original versions of Hack used a range of damage
 * which was similar to, but not identical to the damage used in Advanced
 * Dungeons and Dragons.  I figured that since it was so close, I may as well
 * make it exactly the same as AD&D, adding some more weapons in the process.
 * This has the advantage that it is at least possible that the player would
 * already know the damage of at least some of the weapons.  This was circa
 * 1987 and I used the table from Unearthed Arcana until I got tired of typing
 * them in (leading to something of an imbalance towards weapons early in
 * alphabetical order).  The data structure still doesn't include fields that
 * fully allow the appropriate damage to be described (there's no way to say
 * 3d6 or 1d6+1) so we add on the extra damage in dmgval() if the weapon
 * doesn't do an exact die of damage.
 *
 * Of course new weapons were added later in the development of Nethack.  No
 * AD&D consistency was kept, but most of these don't exist in AD&D anyway.
 *
 * Second edition AD&D came out a few years later; luckily it used the same
 * table.  As of this writing (1999), third edition is in progress but not
 * released.  Let's see if the weapon table stays the same.  --KAA
 * October 2000: It didn't.  Oh, well.
 */

/*
 *	dmgval returns an integer representing the damage bonuses
 *	of "otmp" against the monster.
 */
int
dmgval(otmp, mon)
struct obj *otmp;
struct monst *mon;
{
	int tmp = 0, otyp = otmp->otyp;
	struct permonst *ptr;
	boolean Is_weapon = (otmp->oclass == WEAPON_CLASS || is_weptool(otmp));

	if (!mon) ptr = &mons[NUMMONS];
	else ptr = mon->data;

	if (otyp == CREAM_PIE) return 0;

	if (bigmonst(ptr)) {
		if(otmp->oartifact == ART_VORPAL_BLADE) tmp = exploding_d(2,objects[otyp].oc_wldam,1);
		else if(otmp->oartifact == ART_LUCK_BLADE) tmp = lucky_exploding_d(1,objects[otyp].oc_wldam,0);
	    else if (objects[otyp].oc_wldam) tmp = rnd(objects[otyp].oc_wldam);
	    switch (otyp) {
		case IRON_CHAIN:
		case CROSSBOW_BOLT:
		case MORNING_STAR:
		case PARTISAN:
		case RUNESWORD:
		case ELVEN_BROADSWORD:
		case BROADSWORD:	
			tmp++; 
			break;

		case FLAIL:
		case RANSEUR:
		case SCYTHE:
		case VOULGE:		tmp += rnd(4); break;

		case ACID_VENOM:
		case HALBERD:
		case SPETUM:		tmp += rnd(6); break;

		case BATTLE_AXE:
		case BARDICHE:
		case TRIDENT:		tmp += d(2,4); break;

		case TSURUGI:
		case DWARVISH_MATTOCK:
		case TWO_HANDED_SWORD:	tmp += d(2,6); break;
		case SCIMITAR:
			if(otmp->oartifact == ART_REAVER) tmp += d(1,8); break;
		case LONG_SWORD:	
			if(otmp->oartifact == ART_TOBIUME) tmp -= 3; 
		break;
		case LIGHTSABER:
		case BEAMSWORD:
			tmp += d(2, objects[otyp].oc_wldam); 
			if(otmp->oartifact == ART_ATMA_WEAPON &&
				!Drain_resistance
			){
				tmp += u.ulevel;
				tmp *= Upolyd ?
						((float)u.mh)/u.mhmax  :
						((float)u.uhp)/u.uhpmax;
			}
			break;
		case DOUBLE_LIGHTSABER: 
			tmp += d(2, objects[otyp].oc_wldam); 
			if (otmp->altmode) tmp += d(2, objects[otyp].oc_wldam);
			break;
		case WAR_HAMMER:
			if(otmp->oartifact == ART_MJOLLNIR) tmp += d(2,4); break;
		case BULLWHIP:
			if(otmp->oartifact == ART_VAMPIRE_KILLER) tmp += d(1,10); break;
	    }
	} else {
		if(otmp->oartifact == ART_VORPAL_BLADE) tmp = exploding_d(2,objects[otyp].oc_wsdam,1);
		else if(otmp->oartifact == ART_LUCK_BLADE) tmp = lucky_exploding_d(1,objects[otyp].oc_wsdam,0);
	    else if (objects[otyp].oc_wsdam) tmp = rnd(objects[otyp].oc_wsdam);
	    switch (otyp) {
		case IRON_CHAIN:
		case CROSSBOW_BOLT:
		case MACE:
		case WAR_HAMMER:
		case FLAIL:
		case SPETUM:
		case TRIDENT:		tmp++; if(otmp->oartifact == ART_MJOLLNIR) tmp += d(2,4)+2; break;

		case BULLWHIP:
			if(otmp->oartifact == ART_VAMPIRE_KILLER) tmp += 10; break;

		case LONG_SWORD:	
			if(otmp->oartifact == ART_TOBIUME) tmp -= 2;
		break;
		case BATTLE_AXE:
		case BARDICHE:
		case BILL_GUISARME:
		case GUISARME:
		case LUCERN_HAMMER:
		case MORNING_STAR:
		case RANSEUR:
		case BROADSWORD:
		case ELVEN_BROADSWORD:
		case RUNESWORD:
		case SCYTHE:
		case VOULGE:		
			tmp += rnd(4);
		break;
		case LIGHTSABER:
		case BEAMSWORD:
			tmp += d(2, objects[otyp].oc_wsdam);
			if(otmp->oartifact == ART_ATMA_WEAPON &&
				!Drain_resistance
			){
				tmp += u.ulevel;
				tmp *= Upolyd ?
						((float)u.mh)/u.mhmax  :
						((float)u.uhp)/u.uhpmax;
			}
			break;
		case DOUBLE_LIGHTSABER: 
			tmp += d(2, objects[otyp].oc_wsdam);
			if (otmp->altmode) tmp += d(2, objects[otyp].oc_wsdam);
			break;
		case ACID_VENOM:	tmp += rnd(6); break;
		case SCIMITAR:
			if(otmp->oartifact == ART_REAVER) tmp += d(1,8); break;
	    }
	}
	if (Is_weapon) {
		tmp += otmp->spe;
		/* negative enchantment mustn't produce negative damage */
		if (tmp < 0) tmp = 0;
	}

	if (objects[otyp].oc_material <= LEATHER && thick_skinned(ptr))
		/* thick skinned/scaled creatures don't feel it */
		tmp = 0;
	if (ptr == &mons[PM_SHADE] && objects[otyp].oc_material != SILVER)
		tmp = 0;

	/* "very heavy iron ball"; weight increase is in increments of 160 */
	if (otyp == HEAVY_IRON_BALL && tmp > 0) {
	    int wt = (int)objects[HEAVY_IRON_BALL].oc_weight;

	    if ((int)otmp->owt > wt) {
		wt = ((int)otmp->owt - wt) / 160;
		tmp += rnd(4 * wt);
		if (tmp > 25) tmp = 25;	/* objects[].oc_wldam */
	    }
	}

	if(is_farm(otmp) && ptr->mlet == S_PLANT) tmp *= 2;
/*	Put weapon vs. monster type damage bonuses in below:	*/
	if (Is_weapon || otmp->oclass == GEM_CLASS ||
		otmp->oclass == BALL_CLASS || otmp->oclass == CHAIN_CLASS) {
	    int bonus = 0;

	    if (otmp->blessed && (is_undead(ptr) || is_demon(ptr)))
		bonus += rnd(4);
	    if (is_axe(otmp) && is_wooden(ptr))
		bonus += rnd(4);
	    if (objects[otyp].oc_material == SILVER && hates_silver(ptr))
		bonus += rnd(20);

		if(otmp->oclass == WEAPON_CLASS && objects[(otmp)->otyp].oc_material == WOOD && 
			(otmp->ovar1 & WARD_VEIOISTAFUR) && ptr->mlet == S_EEL) bonus += rnd(20);
			

	    /* if the weapon is going to get a double damage bonus, adjust
	       this bonus so that effectively it's added after the doubling */
	    if (bonus > 1 && otmp->oartifact && spec_dbon(otmp, mon, 25) >= 25)
		bonus = (bonus + 1) / 2;

	    tmp += bonus;
	}

	if (tmp > 0) {
		/* It's debateable whether a rusted blunt instrument
		   should do less damage than a pristine one, since
		   it will hit with essentially the same impact, but
		   there ought to some penalty for using damaged gear
		   so always subtract erosion even for blunt weapons. */
		tmp -= greatest_erosion(otmp);
		if (tmp < 1) tmp = 1;
	}

	return(tmp);
}

#endif /* OVLB */
#ifdef OVL0

STATIC_DCL struct obj *FDECL(oselect, (struct monst *,int));
STATIC_DCL struct obj *FDECL(oselectBoulder, (struct monst *));
#define Oselect(x)	if ((otmp = oselect(mtmp, x)) != 0) return(otmp);

STATIC_OVL struct obj *
oselect(mtmp, x)
struct monst *mtmp;
int x;
{
	struct obj *otmp, *obest = 0;

	for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj) {
	    if (otmp->otyp == x &&
		    /* never select non-cockatrice corpses */
		    !((x == CORPSE || x == EGG) &&
			!touch_petrifies(&mons[otmp->corpsenm])) &&
                    (!is_lightsaber(otmp) || otmp->age) &&
		    (!otmp->oartifact || touch_artifact(otmp,mtmp)))
            {
	        if (!obest ||
		    dmgval(otmp, 0 /*zeromonst*/) > dmgval(obest, 0 /*zeromonst*/))
		    obest = otmp;
	}
	}
	return obest;
}

STATIC_OVL struct obj *
oselectBoulder(mtmp)
struct monst *mtmp;
{
	struct obj *otmp, *obest = 0;

	for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj) {
	    if (is_boulder(otmp)  &&
		    (!otmp->oartifact || touch_artifact(otmp,mtmp)))
            {
	        if (!obest ||
		    dmgval(otmp, 0 /*zeromonst*/) > dmgval(obest, 0 /*zeromonst*/))
		    obest = otmp;
		}
	}
	return obest;
}

static NEARDATA const int rwep[] =
{	LOADSTONE, DWARVISH_SPEAR, SILVER_SPEAR, ELVEN_SPEAR, SPEAR, ORCISH_SPEAR,
	JAVELIN, SHURIKEN, YA, SILVER_ARROW, ELVEN_ARROW, ARROW,
	ORCISH_ARROW, CROSSBOW_BOLT, SILVER_DAGGER, ELVEN_DAGGER, DAGGER,
	ORCISH_DAGGER, KNIFE, FLINT, ROCK, LUCKSTONE, DART,
	/* BOOMERANG, */ CREAM_PIE
};

static NEARDATA const int pwep[] =
{	HALBERD, BARDICHE, SPETUM, BILL_GUISARME, VOULGE, RANSEUR, GUISARME,
	GLAIVE, LUCERN_HAMMER, BEC_DE_CORBIN, FAUCHARD, PARTISAN, LANCE
};

boolean
would_prefer_rwep(mtmp, otmp)
struct monst *mtmp;
struct obj *otmp;
{
    struct obj *wep = select_rwep(mtmp);

    int i = 0;
    
    if (wep)
    {
        if (wep == otmp) return TRUE;

        if (wep->oartifact) return FALSE;

        if (throws_rocks(mtmp->data) &&  is_boulder(wep)) return FALSE;
        if (throws_rocks(mtmp->data) && is_boulder(otmp)) return TRUE;
    }
    
    if (((strongmonst(mtmp->data) && (mtmp->misc_worn_check & W_ARMS) == 0)
	    || !objects[pwep[i]].oc_bimanual) &&
        (objects[pwep[i]].oc_material != SILVER
 	    || !hates_silver(mtmp->data)))
    {
        for (i = 0; i < SIZE(pwep); i++)
        {
            if ( wep &&
	         wep->otyp == pwep[i] &&
               !(otmp->otyp == pwep[i] &&
	         dmgval(otmp, 0 /*zeromonst*/) > dmgval(wep, 0 /*zeromonst*/)))
	        return FALSE;
            if (otmp->otyp == pwep[i]) return TRUE;
        }
    }

    if (is_pole(otmp)) return FALSE; /* If we get this far,
                                        we failed the polearm strength check */

    for (i = 0; i < SIZE(rwep); i++)
    {
        if ( wep &&
             wep->otyp == rwep[i] &&
           !(otmp->otyp == rwep[i] &&
	     dmgval(otmp, 0 /*zeromonst*/) > dmgval(wep, 0 /*zeromonst*/)))
	    return FALSE;
        if (otmp->otyp == rwep[i]) return TRUE;
    }

    return FALSE;
}

struct obj *propellor;

struct obj *
select_rwep(mtmp)	/* select a ranged weapon for the monster */
register struct monst *mtmp;
{
	register struct obj *otmp;
	int i;

	struct obj *tmpprop = &zeroobj;

	char mlet = mtmp->data->mlet;

	propellor = &zeroobj;
	Oselect(EGG); /* cockatrice egg */
	if(throws_rocks(mtmp->data))	/* ...boulders for giants */
	    oselectBoulder(mtmp);

	/* Select polearms first; they do more damage and aren't expendable */
	/* The limit of 13 here is based on the monster polearm range limit
	 * (defined as 5 in mthrowu.c).  5 corresponds to a distance of 2 in
	 * one direction and 1 in another; one space beyond that would be 3 in
	 * one direction and 2 in another; 3^2+2^2=13.
	 */
	/* This check is disabled, as it's targeted towards attacking you
	   and not any arbitrary target. */
	/* if (dist2(mtmp->mx, mtmp->my, mtmp->mux, mtmp->muy) <= 13 && couldsee(mtmp->mx, mtmp->my)) */
	{
	    for (i = 0; i < SIZE(pwep); i++) {
		/* Only strong monsters can wield big (esp. long) weapons.
		 * Big weapon is basically the same as bimanual.
		 * All monsters can wield the remaining weapons.
		 */
		if (((strongmonst(mtmp->data) && (mtmp->misc_worn_check & W_ARMS) == 0)
			|| !objects[pwep[i]].oc_bimanual) &&
		    (objects[pwep[i]].oc_material != SILVER
			|| !hates_silver(mtmp->data))) {
		    if ((otmp = oselect(mtmp, pwep[i])) != 0) {
			propellor = otmp; /* force the monster to wield it */
			return otmp;
		    }
		}
	    }
	}

	/*
	 * other than these two specific cases, always select the
	 * most potent ranged weapon to hand.
	 */
	for (i = 0; i < SIZE(rwep); i++) {
	    int prop;

	    /* shooting gems from slings; this goes just before the darts */
	    /* (shooting rocks is already handled via the rwep[] ordering) */
	    if (rwep[i] == DART && !likes_gems(mtmp->data) &&
		    m_carrying(mtmp, SLING)) {		/* propellor */
		for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
		    if (otmp->oclass == GEM_CLASS &&
			    (otmp->otyp != LOADSTONE || !otmp->cursed)) {
			propellor = m_carrying(mtmp, SLING);
			return otmp;
		    }
	    }

		/* KMH -- This belongs here so darts will work */
	    propellor = &zeroobj;

	    prop = (objects[rwep[i]]).oc_skill;
	    if (prop < 0) {
		switch (-prop) {
		case P_BOW:
		  propellor = (oselect(mtmp, YUMI));
		  if (!propellor) propellor = (oselect(mtmp, ELVEN_BOW));
		  if (!propellor) propellor = (oselect(mtmp, BOW));
		  if (!propellor) propellor = (oselect(mtmp, ORCISH_BOW));
		  break;
		case P_SLING:
		  propellor = (oselect(mtmp, SLING));
		  break;
		case P_CROSSBOW:
		  propellor = (oselect(mtmp, CROSSBOW));
		}
		if (!tmpprop) tmpprop = propellor;
		if (((otmp = MON_WEP(mtmp)) && otmp->cursed && otmp != propellor
				&& mtmp->weapon_check == NO_WEAPON_WANTED) || (mtmp->combat_mode == HNDHND_MODE))
			propellor = 0;
	    }
	    /* propellor = obj, propellor to use
	     * propellor = &zeroobj, doesn't need a propellor
	     * propellor = 0, needed one and didn't have one
	     */
	    if (propellor != 0) {
		/* Note: cannot use m_carrying for loadstones, since it will
		 * always select the first object of a type, and maybe the
		 * monster is carrying two but only the first is unthrowable.
		 */
		if (rwep[i] != LOADSTONE) {
			/* Don't throw a cursed weapon-in-hand or an artifact */
			if ((otmp = oselect(mtmp, rwep[i])) && !otmp->oartifact
			    && (!otmp->cursed || otmp != MON_WEP(mtmp)))
				return(otmp);
		} else for(otmp=mtmp->minvent; otmp; otmp=otmp->nobj) {
		    if (otmp->otyp == LOADSTONE && !otmp->cursed)
			return otmp;
		}
	    }
	  }

	/* failure */
	if (tmpprop) propellor = tmpprop;
	return (struct obj *)0;
}

/* Weapons in order of preference */
static const NEARDATA short hwep[] = {
	  CORPSE,  /* cockatrice corpse */
	  TSURUGI, RUNESWORD, DWARVISH_MATTOCK, TWO_HANDED_SWORD, BATTLE_AXE,
	  DOUBLE_LIGHTSABER, BEAMSWORD,
	  LIGHTSABER,
	  KATANA, UNICORN_HORN, CRYSKNIFE, TRIDENT, LONG_SWORD,
	  ELVEN_BROADSWORD, BROADSWORD, SCIMITAR, SILVER_SABER,
	  MORNING_STAR, ELVEN_SHORT_SWORD, DWARVISH_SHORT_SWORD, SHORT_SWORD,
	  ORCISH_SHORT_SWORD, MACE, AXE, DWARVISH_SPEAR, SILVER_SPEAR,
	  ELVEN_SPEAR, SPEAR, ORCISH_SPEAR, FLAIL, BULLWHIP, QUARTERSTAFF,
	  JAVELIN, AKLYS, CLUB, PICK_AXE,
	  WAR_HAMMER, SILVER_DAGGER, ELVEN_DAGGER, STILETTO, DAGGER, ORCISH_DAGGER,
	  ATHAME, SCALPEL, KNIFE, WORM_TOOTH
	};

boolean
would_prefer_hwep(mtmp, otmp)
struct monst *mtmp;
struct obj *otmp;
{
    struct obj *wep = select_hwep(mtmp);

    int i = 0;
    
    if (wep)
    { 
        if (wep == otmp) return TRUE;
    
        if (wep->oartifact) return FALSE;

        if (is_giant(mtmp->data) &&  wep->otyp == CLUB) return FALSE;
        if (is_giant(mtmp->data) && otmp->otyp == CLUB) return TRUE;
    }
    
    for (i = 0; i < SIZE(hwep); i++)
    {
	if (hwep[i] == CORPSE && !(mtmp->misc_worn_check & W_ARMG))
	    continue;

        if ( wep &&
	     wep->otyp == hwep[i] &&
           !(otmp->otyp == hwep[i] &&
	     dmgval(otmp, 0 /*zeromonst*/) > dmgval(wep, 0 /*zeromonst*/)))
	    return FALSE;
        if (otmp->otyp == hwep[i]) return TRUE;
    }

    return FALSE;
}

struct obj *
select_hwep(mtmp)	/* select a hand to hand weapon for the monster */
register struct monst *mtmp;
{
	register struct obj *otmp;
	register int i;
	boolean strong = strongmonst(mtmp->data);
	boolean wearing_shield = (mtmp->misc_worn_check & W_ARMS) != 0;

	/* prefer artifacts to everything else */
	for(otmp=mtmp->minvent; otmp; otmp = otmp->nobj) {
		if (otmp->oclass == WEAPON_CLASS
			&& otmp->oartifact && touch_artifact(otmp,mtmp)
			&& ((strong && !wearing_shield)
			    || !objects[otmp->otyp].oc_bimanual))
		    return otmp;
	}

	if(is_giant(mtmp->data))	/* giants just love to use clubs */
	    Oselect(CLUB);

	/* only strong monsters can wield big (esp. long) weapons */
	/* big weapon is basically the same as bimanual */
	/* all monsters can wield the remaining weapons */
	for (i = 0; i < SIZE(hwep); i++) {
	    if (hwep[i] == CORPSE && !(mtmp->misc_worn_check & W_ARMG))
		continue;
	    if (((strong && !wearing_shield)
			|| !objects[hwep[i]].oc_bimanual) &&
		    (objects[hwep[i]].oc_material != SILVER
			|| !hates_silver(mtmp->data)))
		Oselect(hwep[i]);
	}

	/* failure */
	return (struct obj *)0;
}

/* Called after polymorphing a monster, robbing it, etc....  Monsters
 * otherwise never unwield stuff on their own.  Might print message.
 */
void
possibly_unwield(mon, polyspot)
struct monst *mon;
boolean polyspot;
{
	struct obj *obj, *mw_tmp;

	if (!(mw_tmp = MON_WEP(mon)))
		return;
	for (obj = mon->minvent; obj; obj = obj->nobj)
		if (obj == mw_tmp) break;
	if (!obj) { /* The weapon was stolen or destroyed */
		MON_NOWEP(mon);
		mon->weapon_check = NEED_WEAPON;
		return;
	}
	if (!attacktype(mon->data, AT_WEAP)) {
		setmnotwielded(mon, mw_tmp);
		MON_NOWEP(mon);
		mon->weapon_check = NO_WEAPON_WANTED;
		obj_extract_self(obj);
		if (cansee(mon->mx, mon->my)) {
		    pline("%s drops %s.", Monnam(mon),
			  distant_name(obj, doname));
		    newsym(mon->mx, mon->my);
		}
		/* might be dropping object into water or lava */
		if (!flooreffects(obj, mon->mx, mon->my, "drop")) {
		    if (polyspot) bypass_obj(obj);
		    place_object(obj, mon->mx, mon->my);
		    stackobj(obj);
		}
		return;
	}
	/* The remaining case where there is a change is where a monster
	 * is polymorphed into a stronger/weaker monster with a different
	 * choice of weapons.  This has no parallel for players.  It can
	 * be handled by waiting until mon_wield_item is actually called.
	 * Though the monster still wields the wrong weapon until then,
	 * this is OK since the player can't see it.  (FIXME: Not okay since
	 * probing can reveal it.)
	 * Note that if there is no change, setting the check to NEED_WEAPON
	 * is harmless.
	 * Possible problem: big monster with big cursed weapon gets
	 * polymorphed into little monster.  But it's not quite clear how to
	 * handle this anyway....
	 */
	if (!(mw_tmp->cursed && mon->weapon_check == NO_WEAPON_WANTED))
	    mon->weapon_check = NEED_WEAPON;
	return;
}

/* Let a monster try to wield a weapon, based on mon->weapon_check.
 * Returns 1 if the monster took time to do it, 0 if it did not.
 */
int
mon_wield_item(mon)
register struct monst *mon;
{
	struct obj *obj;

	/* This case actually should never happen */
	if (mon->weapon_check == NO_WEAPON_WANTED) return 0;
	switch(mon->weapon_check) {
		case NEED_HTH_WEAPON:
			obj = select_hwep(mon);
			break;
		case NEED_RANGED_WEAPON:
			(void)select_rwep(mon);
			obj = propellor;
			break;
		case NEED_PICK_AXE:
			obj = m_carrying(mon, PICK_AXE);
			/* KMH -- allow other picks */
			if (!obj && !which_armor(mon, W_ARMS))
			    obj = m_carrying(mon, DWARVISH_MATTOCK);
			break;
		case NEED_AXE:
			/* currently, only 2 types of axe */
			obj = m_carrying(mon, BATTLE_AXE);
			if (!obj || which_armor(mon, W_ARMS))
			    obj = m_carrying(mon, AXE);
			break;
		case NEED_PICK_OR_AXE:
			/* prefer pick for fewer switches on most levels */
			obj = m_carrying(mon, DWARVISH_MATTOCK);
			if (!obj) obj = m_carrying(mon, BATTLE_AXE);
			if (!obj || which_armor(mon, W_ARMS)) {
			    obj = m_carrying(mon, PICK_AXE);
			    if (!obj) obj = m_carrying(mon, AXE);
			}
			break;
		default: impossible("weapon_check %d for %s?",
				mon->weapon_check, mon_nam(mon));
			return 0;
	}
	if (obj && obj != &zeroobj) {
		struct obj *mw_tmp = MON_WEP(mon);
		if (mw_tmp && mw_tmp->otyp == obj->otyp) {
		/* already wielding one similar to it */
			if (is_lightsaber(obj))
			    mon_ignite_lightsaber(obj, mon);
			mon->weapon_check = NEED_WEAPON;
			return 0;
		}
		/* Actually, this isn't necessary--as soon as the monster
		 * wields the weapon, the weapon welds itself, so the monster
		 * can know it's cursed and needn't even bother trying.
		 * Still....
		 */
		if (mw_tmp && mw_tmp->cursed && mw_tmp->otyp != CORPSE) {
		    if (canseemon(mon)) {
			char welded_buf[BUFSZ];
			const char *mon_hand = mbodypart(mon, HAND);

			if (bimanual(mw_tmp)) mon_hand = makeplural(mon_hand);
			Sprintf(welded_buf, "%s welded to %s %s",
				otense(mw_tmp, "are"),
				mhis(mon), mon_hand);

			if (obj->otyp == PICK_AXE) {
			    pline("Since %s weapon%s %s,",
				  s_suffix(mon_nam(mon)),
				  plur(mw_tmp->quan), welded_buf);
			    pline("%s cannot wield that %s.",
				mon_nam(mon), xname(obj));
			} else {
			    pline("%s tries to wield %s.", Monnam(mon),
				doname(obj));
			    pline("%s %s %s!",
				  s_suffix(Monnam(mon)),
				  xname(mw_tmp), welded_buf);
			}
			mw_tmp->bknown = 1;
		    }
		    mon->weapon_check = NO_WEAPON_WANTED;
		    return 1;
		}
		mon->mw = obj;		/* wield obj */
		setmnotwielded(mon, mw_tmp);
		mon->weapon_check = NEED_WEAPON;
		if (canseemon(mon)) {
		    pline("%s wields %s%s", Monnam(mon), doname(obj),
		          mon->mtame ? "." : "!");
		    if (obj->cursed && obj->otyp != CORPSE) {
			pline("%s %s to %s %s!",
			    Tobjnam(obj, "weld"),
			    is_plural(obj) ? "themselves" : "itself",
			    s_suffix(mon_nam(mon)), mbodypart(mon,HAND));
			obj->bknown = 1;
		    }
		}
		if (artifact_light(obj) && !obj->lamplit) {
		    begin_burn(obj, FALSE);
		    if (canseemon(mon))
				pline("%s %s%s in %s %s!",
					Tobjnam(obj, (obj->blessed ? "shine" : "glow")), 
					(obj->blessed ? " very" : ""),
					(obj->cursed ? "" : " brilliantly"),
			    s_suffix(mon_nam(mon)), mbodypart(mon,HAND));
		}
		obj->owornmask = W_WEP;
		if (is_lightsaber(obj))
		    mon_ignite_lightsaber(obj, mon);
		return 1;
	}
	mon->weapon_check = NEED_WEAPON;
	return 0;
}

static void
mon_ignite_lightsaber(obj, mon)
struct obj * obj;
struct monst * mon;
{
	/* No obj or not lightsaber */
	if (!obj || !is_lightsaber(obj)) return;

	/* WAC - Check lightsaber is on */
	if (!obj->lamplit) {
	    if (obj->cursed && !rn2(2)) {
		if (canseemon(mon)) pline("%s %s flickers and goes out.", 
			s_suffix(Monnam(mon)), xname(obj));

	    } else {
		if (canseemon(mon)) {
			makeknown(obj->otyp);
			pline("%s ignites %s.", Monnam(mon),
				an(xname(obj)));
		}	    	
		begin_burn(obj, FALSE);
	    }
	} else {
		/* Double Lightsaber in single mode? Ignite second blade */
		if (obj->otyp == DOUBLE_LIGHTSABER && !obj->altmode) {
		    /* Do we want to activate dual bladed mode? */
		    if (!obj->altmode && (!obj->cursed || rn2(4))) {
			if (canseemon(mon)) pline("%s ignites the second blade of %s.", 
				Monnam(mon), an(xname(obj)));
		    	obj->altmode = TRUE;
		    	return;
		    } else obj->altmode = FALSE;
		    lightsaber_deactivate(obj, TRUE);
		}
		return;
	}
}
int
abon()		/* attack bonus for strength & dexterity */
{
	int sbon;
	int str = ACURR(A_STR), dex = ACURR(A_DEX);

	if (Upolyd) return(adj_lev(&mons[u.umonnum]) - 3);
	if (str < 6) sbon = -2;
	else if (str < 8) sbon = -1;
	else if (str < 17) sbon = 0;
	else if (str <= STR18(50)) sbon = 1;	/* up to 18/50 */
	else if (str < STR18(100)) sbon = 2;
	else sbon = 3;

/* Game tuning kludge: make it a bit easier for a low level character to hit */
	sbon += (u.ulevel < 3) ? 1 : 0;

	if (dex < 4) return(sbon-3);
	else if (dex < 6) return(sbon-2);
	else if (dex < 8) return(sbon-1);
	else if (dex < 14) return(sbon);
	else return(sbon + dex-14);
}

#endif /* OVL0 */
#ifdef OVL1

int
dbon(otmp)		/* damage bonus for strength */
struct obj *otmp;
{
	int str = ACURR(A_STR);
	int bonus = 0;

	if (Upolyd) return(0);

	if (str < 6) bonus = -6+str;
	else if (str < 16) bonus = 0;
	else if (str < 18) bonus = 1;
	else if (str == 18) bonus = 2;		/* up to 18 */
	else if (str <= STR18(75)) bonus = 3;		/* up to 18/75 */
	else if (str <= STR18(90)) bonus = 4;		/* up to 18/90 */
	else if (str < STR18(100)) bonus = 5;		/* up to 18/99 */
	else if (str < 22) bonus = 6;
	else if (str < 25) bonus = 7;
	else /*  str ==25*/bonus = 8;

	if(otmp && objects[otmp->otyp].oc_bimanual) bonus *= 2;
	return bonus;
}


/* copy the skill level name into the given buffer */
STATIC_OVL char *
skill_level_name(skill, buf)
int skill;
char *buf;
{
    const char *ptr;

    switch (P_SKILL(skill)) {
	case P_UNSKILLED:    ptr = "Unskilled"; break;
	case P_BASIC:	     ptr = "Basic";     break;
	case P_SKILLED:	     ptr = "Skilled";   break;
	case P_EXPERT:	     ptr = "Expert";    break;
	/* these are for unarmed combat/martial arts only */
	case P_MASTER:	     ptr = "Master";    break;
	case P_GRAND_MASTER: ptr = "Grand Master"; break;
	default:	     ptr = "Unknown";	break;
    }
    Strcpy(buf, ptr);
    return buf;
}

/* return the # of slots required to advance the skill */
STATIC_OVL int
slots_required(skill)
int skill;
{
    int tmp = OLD_P_SKILL(skill);

    /* The more difficult the training, the more slots it takes.
     *	unskilled -> basic	1
     *	basic -> skilled	2
     *	skilled -> expert	3
     */
    if (skill <= P_LAST_WEAPON || skill == P_TWO_WEAPON_COMBAT)
	return tmp;

    /* Fewer slots used up for unarmed or martial.
     *	unskilled -> basic	1
     *	basic -> skilled	1
     *	skilled -> expert	2
     *	expert -> master	2
     *	master -> grand master	3
     */
    return (tmp + 1) / 2;
}

/* return true if this skill can be advanced */
/*ARGSUSED*/
STATIC_OVL boolean
can_advance(skill, speedy)
int skill;
boolean speedy;
{
    return !P_RESTRICTED(skill)
	    && P_SKILL(skill) < P_MAX_SKILL(skill) && (
#ifdef WIZARD
	    (wizard && speedy) ||
#endif
	    (P_ADVANCE(skill) >=
		(unsigned) practice_needed_to_advance(OLD_P_SKILL(skill))
	    && u.skills_advanced < P_SKILL_LIMIT
	    && u.weapon_slots >= slots_required(skill)));
}

/* return true if this skill could be advanced if more slots were available */
STATIC_OVL boolean
could_advance(skill)
int skill;
{
    return !P_RESTRICTED(skill)
	    && P_SKILL(skill) < P_MAX_SKILL(skill) && (
	    (P_ADVANCE(skill) >=
		(unsigned) practice_needed_to_advance(OLD_P_SKILL(skill))
	    && u.skills_advanced < P_SKILL_LIMIT));
}

/* return true if this skill has reached its maximum and there's been enough
   practice to become eligible for the next step if that had been possible */
STATIC_OVL boolean
peaked_skill(skill)
int skill;
{
    return !P_RESTRICTED(skill)
	    && P_SKILL(skill) >= P_MAX_SKILL(skill) && (
	    (P_ADVANCE(skill) >=
		(unsigned) practice_needed_to_advance(OLD_P_SKILL(skill))));
}

STATIC_OVL void
skill_advance(skill)
int skill;
{
    u.weapon_slots -= slots_required(skill);
	OLD_P_SKILL(skill)++;
    u.skill_record[u.skills_advanced++] = skill;
    /* subtly change the advance message to indicate no more advancement */
    You("are now %s skilled in %s.",
	P_SKILL(skill) >= P_MAX_SKILL(skill) ? "most" : "more",
	P_NAME(skill));
}

const static struct skill_range {
	short first, last;
	const char *name;
} skill_ranges[] = {
    { P_FIRST_H_TO_H, P_LAST_H_TO_H, "Fighting Skills" },
    { P_FIRST_WEAPON, P_LAST_WEAPON, "Weapon Skills" },
    { P_FIRST_SPELL,  P_LAST_SPELL,  "Spellcasting Skills" },
};

/*
 * The `#enhance' extended command.  What we _really_ would like is
 * to keep being able to pick things to advance until we couldn't any
 * more.  This is currently not possible -- the menu code has no way
 * to call us back for instant action.  Even if it did, we would also need
 * to be able to update the menu since selecting one item could make
 * others unselectable.
 */
int
enhance_weapon_skill()
#ifdef DUMP_LOG
{
	return enhance_skill(FALSE);
}

void dump_weapon_skill()
{
	enhance_skill(TRUE);
}

int enhance_skill(boolean want_dump)
/* This is the original enhance_weapon_skill() function slightly modified
 * to write the skills to the dump file. I added the wrapper functions just
 * because it looked like the easiest way to add a parameter to the
 * function call. - Jukka Lahtinen, August 2001
 */
#endif
{
    int pass, i, n, len, longest,
	to_advance, eventually_advance, maxxed_cnt;
    char buf[BUFSZ], sklnambuf[BUFSZ];
    const char *prefix;
    menu_item *selected;
    anything any;
    winid win;
    boolean speedy = FALSE;
#ifdef DUMP_LOG
    char buf2[BUFSZ];
    boolean logged;
#endif

#ifdef WIZARD
#ifdef DUMP_LOG
	if (!want_dump)
#endif
	if (wizard && yn("Advance skills without practice?") == 'y')
	    speedy = TRUE;
#endif

	do {
	    /* find longest available skill name, count those that can advance */
	    to_advance = eventually_advance = maxxed_cnt = 0;
	    for (longest = 0, i = 0; i < P_NUM_SKILLS; i++) {
		if (P_RESTRICTED(i)) continue;
		if ((len = strlen(P_NAME(i))) > longest)
		    longest = len;
		if (can_advance(i, speedy)) to_advance++;
		else if (could_advance(i)) eventually_advance++;
		else if (peaked_skill(i)) maxxed_cnt++;
	    }

#ifdef DUMP_LOG
	    if (want_dump)
		dump("","Your skills at the end");
	    else {
#endif
	    win = create_nhwindow(NHW_MENU);
	    start_menu(win);

	    /* start with a legend if any entries will be annotated
	       with "*" or "#" below */
	    if (eventually_advance > 0 || maxxed_cnt > 0) {
		any.a_void = 0;
		if (eventually_advance > 0) {
		    Sprintf(buf,
			    "(Skill%s flagged by \"*\" may be enhanced %s.)",
			    plur(eventually_advance),
			    (u.ulevel < MAXULEV) ?
				"when you're more experienced" :
				"if skill slots become available");
		    add_menu(win, NO_GLYPH, &any, 0, 0, ATR_NONE,
			     buf, MENU_UNSELECTED);
		}
		if (maxxed_cnt > 0) {
		    Sprintf(buf,
		  "(Skill%s flagged by \"#\" cannot be enhanced any further.)",
			    plur(maxxed_cnt));
		    add_menu(win, NO_GLYPH, &any, 0, 0, ATR_NONE,
			     buf, MENU_UNSELECTED);
		}
		add_menu(win, NO_GLYPH, &any, 0, 0, ATR_NONE,
			     "", MENU_UNSELECTED);
	    }
#ifdef DUMP_LOG
	    } /* want_dump or not */
#endif

	    /* List the skills, making ones that could be advanced
	       selectable.  List the miscellaneous skills first.
	       Possible future enhancement:  list spell skills before
	       weapon skills for spellcaster roles. */
	  for (pass = 0; pass < SIZE(skill_ranges); pass++)
	    for (i = skill_ranges[pass].first;
		 i <= skill_ranges[pass].last; i++) {
		/* Print headings for skill types */
		any.a_void = 0;
		if (i == skill_ranges[pass].first)
#ifdef DUMP_LOG
		if (want_dump) {
		    dump("  ",(char *)skill_ranges[pass].name);
		    logged=FALSE;
		} else
#endif
		    add_menu(win, NO_GLYPH, &any, 0, 0, iflags.menu_headings,
			     skill_ranges[pass].name, MENU_UNSELECTED);
#ifdef DUMP_LOG
		if (want_dump) {
		    if (P_SKILL(i) > P_UNSKILLED) {
		 	Sprintf(buf2,"%-*s [%s]",
			    longest, P_NAME(i),skill_level_name(i, buf));
			dump("    ",buf2);
			logged=TRUE;
		    } else if (i == skill_ranges[pass].last && !logged) {
			dump("    ","(none)");
		    }
               } else {
#endif

		if (P_RESTRICTED(i)) continue;
		/*
		 * Sigh, this assumes a monospaced font unless
		 * iflags.menu_tab_sep is set in which case it puts
		 * tabs between columns.
		 * The 12 is the longest skill level name.
		 * The "    " is room for a selection letter and dash, "a - ".
		 */
		if (can_advance(i, speedy))
		    prefix = "";	/* will be preceded by menu choice */
		else if (could_advance(i))
		    prefix = "  * ";
		else if (peaked_skill(i))
		    prefix = "  # ";
		else
		    prefix = (to_advance + eventually_advance +
				maxxed_cnt > 0) ? "    " : "";
		(void) skill_level_name(i, sklnambuf);
#ifdef WIZARD
		if (wizard) {
		    if (!iflags.menu_tab_sep)
			Sprintf(buf, " %s%-*s %-12s %5d(%4d)",
			    prefix, longest, P_NAME(i), sklnambuf,
			    P_ADVANCE(i),
			    practice_needed_to_advance(OLD_P_SKILL(i)));
		    else
			Sprintf(buf, " %s%s\t%s\t%5d(%4d)",
			    prefix, P_NAME(i), sklnambuf,
			    P_ADVANCE(i),
			    practice_needed_to_advance(OLD_P_SKILL(i)));
		 } else
#endif
		{
		    if (!iflags.menu_tab_sep)
			Sprintf(buf, " %s %-*s [%s]",
			    prefix, longest, P_NAME(i), sklnambuf);
		    else
			Sprintf(buf, " %s%s\t[%s]",
			    prefix, P_NAME(i), sklnambuf);
		}
		any.a_int = can_advance(i, speedy) ? i+1 : 0;
		add_menu(win, NO_GLYPH, &any, 0, 0, ATR_NONE,
			 buf, MENU_UNSELECTED);
#ifdef DUMP_LOG
		} /* !want_dump */
#endif
	    }

	    Strcpy(buf, (to_advance > 0) ? "Pick a skill to advance:" :
					   "Current skills:");
#ifdef WIZARD
	    if (wizard && !speedy)
		Sprintf(eos(buf), "  (%d slot%s available)",
			u.weapon_slots, plur(u.weapon_slots));
#endif
#ifdef DUMP_LOG
	    if (want_dump) {
		dump("","");
		n=0;
	    } else {
#endif
	    end_menu(win, buf);
	    n = select_menu(win, to_advance ? PICK_ONE : PICK_NONE, &selected);
	    destroy_nhwindow(win);
	    if (n > 0) {
		n = selected[0].item.a_int - 1;	/* get item selected */
		free((genericptr_t)selected);
		skill_advance(n);
		/* check for more skills able to advance, if so then .. */
		for (n = i = 0; i < P_NUM_SKILLS; i++) {
		    if (can_advance(i, speedy)) {
			if (!speedy) You_feel("you could be more dangerous!");
			n++;
			break;
		    }
		}
	    }
#ifdef DUMP_LOG
	    }
#endif
	} while (speedy && n > 0);
	return 0;
}

/*
 * Change from restricted to unrestricted, allowing P_BASIC as max.  This
 * function may be called with with P_NONE.  Used in pray.c.
 */
void
unrestrict_weapon_skill(skill)
int skill;
{
    if (skill < P_NUM_SKILLS && OLD_P_RESTRICTED(skill)) {
		OLD_P_SKILL(skill) = P_UNSKILLED;
		OLD_P_MAX_SKILL(skill) = P_BASIC;
	P_ADVANCE(skill) = 0;
    }
}

#endif /* OVL1 */
#ifdef OVLB

void
use_skill(skill,degree)
int skill;
int degree;
{
    boolean advance_before;

    if (skill != P_NONE && !P_RESTRICTED(skill)) {
	advance_before = can_advance(skill, FALSE);
	P_ADVANCE(skill)+=degree;
	if (!advance_before && can_advance(skill, FALSE))
	    give_may_advance_msg(skill);
    }
}

void
add_weapon_skill(n)
int n;	/* number of slots to gain; normally one */
{
    int i, before, after;

    for (i = 0, before = 0; i < P_NUM_SKILLS; i++)
	if (can_advance(i, FALSE)) before++;
    u.weapon_slots += n;
    for (i = 0, after = 0; i < P_NUM_SKILLS; i++)
	if (can_advance(i, FALSE)) after++;
    if (before < after)
	give_may_advance_msg(P_NONE);
}

void
lose_weapon_skill(n)
int n;	/* number of slots to lose; normally one */
{
    int skill;

    while (--n >= 0) {
	/* deduct first from unused slots, then from last placed slot, if any */
	if (u.weapon_slots) {
	    u.weapon_slots--;
	} else if (u.skills_advanced) {
	    skill = u.skill_record[--u.skills_advanced];
	    if (OLD_P_SKILL(skill) <= P_UNSKILLED)
		panic("lose_weapon_skill (%d)", skill);
	    OLD_P_SKILL(skill)--;	/* drop skill one level */
	    /* Lost skill might have taken more than one slot; refund rest. */
	    u.weapon_slots = slots_required(skill) - 1;
	    /* It might now be possible to advance some other pending
	       skill by using the refunded slots, but giving a message
	       to that effect would seem pretty confusing.... */
	}
    }
}

int
weapon_type(obj)
struct obj *obj;
{
	/* KMH -- now uses the object table */
	int type;

	if (!obj)
		/* Not using a weapon */
		return (P_BARE_HANDED_COMBAT);
#ifdef CONVICT
    if ((obj->otyp == HEAVY_IRON_BALL) && Role_if(PM_CONVICT))
        return objects[obj->otyp].oc_skill;
#endif /* CONVICT */
	if (obj->oclass != WEAPON_CLASS && obj->oclass != TOOL_CLASS &&
	    obj->oclass != GEM_CLASS)
		/* Not a weapon, weapon-tool, or ammo */
		return (P_NONE);
	if(obj && obj->oartifact && obj->oartifact == ART_SUNSWORD){
		if(P_SKILL(P_LONG_SWORD) > P_SKILL(P_SHORT_SWORD))
			type = P_LONG_SWORD;
		else type = P_SHORT_SWORD;
	}
	else type = objects[obj->otyp].oc_skill;
	return ((type < 0) ? -type : type);
}

int
uwep_skill_type()
{
	if (u.twoweap)
		return P_TWO_WEAPON_COMBAT;
	return weapon_type(uwep);
}

/*
 * Return hit bonus/penalty based on skill of weapon.
 * Treat restricted weapons as unskilled.
 */
int
weapon_hit_bonus(weapon)
struct obj *weapon;
{
    int type, wep_type, skill, bonus = 0;
    static const char bad_skill[] = "weapon_hit_bonus: bad skill %d";

    wep_type = weapon_type(weapon);
    /* use two weapon skill only if attacking with one of the wielded weapons */
    type = (u.twoweap && (weapon == uwep || weapon == uswapwep)) ?
	    P_TWO_WEAPON_COMBAT : wep_type;
    if (type == P_NONE) {
	bonus = 0;
    } else if (type <= P_LAST_WEAPON) {
	switch (P_SKILL(type)) {
	    default: impossible(bad_skill, P_SKILL(type)); /* fall through */
	    case P_ISRESTRICTED:
	    case P_UNSKILLED:   bonus = -4; break;
	    case P_BASIC:       bonus =  0; break;
	    case P_SKILLED:     bonus =  2; break;
	    case P_EXPERT:      bonus =  5; break;
	}
    } else if (type == P_TWO_WEAPON_COMBAT) {
	skill = P_SKILL(P_TWO_WEAPON_COMBAT);
	if (P_SKILL(wep_type) < skill) skill = P_SKILL(wep_type);
	switch (skill) {
	    default: impossible(bad_skill, skill); /* fall through */
	    case P_ISRESTRICTED:
	    case P_UNSKILLED:   bonus = -9; break;
	    case P_BASIC:	bonus = -7; break;
	    case P_SKILLED:	bonus = -5; break;
	    case P_EXPERT:	bonus = -2; break;
	}
    } else if (type == P_BARE_HANDED_COMBAT) {
	/*
	 *	       b.h.  m.a.
	 *	unskl:	+1   n/a
	 *	basic:	+1    +3
	 *	skild:	+2    +4
	 *	exprt:	+2    +5
	 *	mastr:	+3    +6
	 *	grand:	+3    +7
	 */
	bonus = P_SKILL(type);
	bonus = max(bonus,P_UNSKILLED) - 1;	/* unskilled => 0 */
	bonus = ((bonus + 2) * (martial_bonus() ? 2 : 1)) / 2;
    }

#ifdef STEED
	/* KMH -- It's harder to hit while you are riding */
	if (u.usteed) {
		switch (P_SKILL(P_RIDING)) {
		    case P_ISRESTRICTED:
		    case P_UNSKILLED:   bonus -= 2; break;
		    case P_BASIC:       bonus -= 1; break;
		    case P_SKILLED:     break;
		    case P_EXPERT:      bonus += 2; break;//but an expert can use the added momentum
		}
		if (u.twoweap) bonus -= 2;
	}
#endif

    return bonus;
}

/*
 * Return damage bonus/penalty based on skill of weapon.
 * Treat restricted weapons as unskilled.
 */
int
weapon_dam_bonus(weapon)
struct obj *weapon;
{
    int type, wep_type, skill, bonus = 0;

    wep_type = weapon_type(weapon);
    /* use two weapon skill only if attacking with one of the wielded weapons */
    type = (u.twoweap && (weapon == uwep || weapon == uswapwep)) ?
	    P_TWO_WEAPON_COMBAT : wep_type;
    if (type == P_NONE) {
	bonus = 0;
    } else if (type <= P_LAST_WEAPON) {
	switch (P_SKILL(type)) {
	    default: impossible("weapon_dam_bonus: bad skill %d",P_SKILL(type));
		     /* fall through */
	    case P_ISRESTRICTED:	bonus = -5; break;
	    case P_UNSKILLED:	bonus = -2; break;
	    case P_BASIC:	bonus =  0; break;
	    case P_SKILLED:	bonus =  1; break;
	    case P_EXPERT:	bonus =  3; break;
	}
    } else if (type == P_TWO_WEAPON_COMBAT) {
	skill = P_SKILL(P_TWO_WEAPON_COMBAT);
	if (P_SKILL(wep_type) < skill) skill = P_SKILL(wep_type);
	switch (skill) {
	    default:
	    case P_ISRESTRICTED:
	    case P_UNSKILLED:	bonus = -3; break;
	    case P_BASIC:	bonus = -1; break;
	    case P_SKILLED:	bonus = 0; break;
	    case P_EXPERT:	bonus = 2; break;
	}
    } else if (type == P_BARE_HANDED_COMBAT) {
	/*
	 *	       b.h.  m.a.
	 *	unskl:	 0   n/a
	 *	basic:	+1    +3
	 *	skild:	+1    +4
	 *	exprt:	+2    +6
	 *	mastr:	+2    +7
	 *	grand:	+3    +9
	 */
	bonus = P_SKILL(type);
	bonus = max(bonus,P_UNSKILLED) - 1;	/* unskilled => 0 */
	bonus = ((bonus + 1) * (martial_bonus() ? 3 : 1)) / 2;
    }

#ifdef STEED
	/* KMH -- Riding gives some thrusting damage */
	if (u.usteed && type != P_TWO_WEAPON_COMBAT) {
		switch (P_SKILL(P_RIDING)) {
		    case P_ISRESTRICTED:
		    case P_UNSKILLED:   break;
		    case P_BASIC:       break;
		    case P_SKILLED:     bonus += 2; break;
		    case P_EXPERT:      bonus += 5; break;
		}
	}
#endif

    return bonus;
}

/*
 * Initialize weapon skill array for the game.  Start by setting all
 * skills to restricted, then set the skill for every weapon the
 * hero is holding, finally reading the given array that sets
 * maximums.
 */
void
skill_add(class_skill)
const struct def_skill *class_skill;
{
	int skmax, skill;
	/* walk through array to set skill maximums */
	for (; class_skill->skill != P_NONE; class_skill++) {
	    skmax = class_skill->skmax;
	    skill = class_skill->skill;

	    OLD_P_MAX_SKILL(skill) = skmax;
	    if (OLD_P_SKILL(skill) == P_ISRESTRICTED)	/* skill pre-set */
			OLD_P_SKILL(skill) = P_UNSKILLED;
	}
	/*
	 * Make sure we haven't missed setting the max on a skill
	 * & set advance
	 */
	for (skill = 0; skill < P_NUM_SKILLS; skill++) {
	    if (!OLD_P_RESTRICTED(skill)) {
		if (OLD_P_MAX_SKILL(skill) < OLD_P_SKILL(skill)) {
		    impossible("skill_init: curr > max: %s", P_NAME(skill));
		    OLD_P_MAX_SKILL(skill) = OLD_P_SKILL(skill);
		}
		P_ADVANCE(skill) = practice_needed_to_advance(OLD_P_SKILL(skill)-1);
	    }
	}
}
void
skill_init(class_skill)
const struct def_skill *class_skill;
{
	struct obj *obj;
	int skmax, skill;

	/* initialize skill array; by default, everything is restricted */
	for (skill = 0; skill < P_NUM_SKILLS; skill++) {
		OLD_P_SKILL(skill) = P_ISRESTRICTED;
		OLD_P_MAX_SKILL(skill) = P_ISRESTRICTED;
	    P_ADVANCE(skill) = 0;
	}

	/* Set skill for all weapons in inventory to be basic */
	if(!Role_if(PM_EXILE)) for (obj = invent; obj; obj = obj->nobj) {
	    skill = weapon_type(obj);
	    if (skill != P_NONE)
			OLD_P_SKILL(skill) = P_BASIC;
	}

	/* set skills for magic */
	if (Role_if(PM_HEALER) || Role_if(PM_MONK)) {
		OLD_P_SKILL(P_HEALING_SPELL) = P_BASIC;
	} else if (Role_if(PM_PRIEST)) {
		OLD_P_SKILL(P_CLERIC_SPELL) = P_BASIC;
	} else if (Role_if(PM_WIZARD)) {
		OLD_P_SKILL(P_ATTACK_SPELL) = P_BASIC;
		OLD_P_SKILL(P_ENCHANTMENT_SPELL) = P_BASIC;
	}

	/* walk through array to set skill maximums */
	for (; class_skill->skill != P_NONE; class_skill++) {
	    skmax = class_skill->skmax;
	    skill = class_skill->skill;

	    OLD_P_MAX_SKILL(skill) = skmax;
	    if (OLD_P_SKILL(skill) == P_ISRESTRICTED)	/* skill pre-set */
			OLD_P_SKILL(skill) = P_UNSKILLED;
	}

	/* High potential fighters already know how to use their hands. */
	if (OLD_P_MAX_SKILL(P_BARE_HANDED_COMBAT) > P_EXPERT)
	    OLD_P_SKILL(P_BARE_HANDED_COMBAT) = P_BASIC;

	/* Roles that start with a horse know how to ride it */
#ifdef STEED
	if (urole.petnum == PM_PONY)
	    OLD_P_SKILL(P_RIDING) = P_BASIC;
#endif

	/*
	 * Make sure we haven't missed setting the max on a skill
	 * & set advance
	 */
	for (skill = 0; skill < P_NUM_SKILLS; skill++) {
	    if (!OLD_P_RESTRICTED(skill)) {
		if (OLD_P_MAX_SKILL(skill) < OLD_P_SKILL(skill)) {
		    impossible("skill_init: curr > max: %s", P_NAME(skill));
		    OLD_P_MAX_SKILL(skill) = OLD_P_SKILL(skill);
		}
		P_ADVANCE(skill) = practice_needed_to_advance(OLD_P_SKILL(skill)-1);
	    }
	}
}

void
setmnotwielded(mon,obj)
register struct monst *mon;
register struct obj *obj;
{
    if (!obj) return;
    if (artifact_light(obj) && obj->lamplit) {
	end_burn(obj, FALSE);
	if (canseemon(mon))
	    pline("%s in %s %s %s glowing.", The(xname(obj)),
		  s_suffix(mon_nam(mon)), mbodypart(mon,HAND),
		  otense(obj, "stop"));
    }
    obj->owornmask &= ~W_WEP;
}

#endif /* OVLB */

/*weapon.c*/
