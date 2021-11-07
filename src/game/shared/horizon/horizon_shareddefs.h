//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HORIZON_SHAREDDEFS_H
#define HORIZON_SHAREDDEFS_H

#define HORIZON_MAX_PLAYERS 48
#define HORIZON_PLAYER_MODEL "models/players/player.mdl"
#define HORIZON_DEFAULT_PLAYER_RUNSPEED			320

enum
{
	PAINT_TYPE_BOUNCE = 0,
	PAINT_TYPE_SPEED,
	PAINT_TYPE_STICK,
	PAINT_TYPE_PORTAL
};

static const char* pszPaintStrings[] =
{
	"PAINT_TYPE_BOUNCE",
	"PAINT_TYPE_SPEED",
	"PAINT_TYPE_STICK",
	"PAINT_TYPE_PORTAL"
};

inline int UTIL_NextPaintType(int paintType)
{
	return paintType == PAINT_TYPE_BOUNCE ? PAINT_TYPE_PORTAL : paintType - 1;
}

inline int UTIL_PrevPaintType(int paintType)
{
	return paintType == PAINT_TYPE_PORTAL ? PAINT_TYPE_BOUNCE : paintType + 1;
}

#endif // HORIZON_SHAREDDEFS_H
