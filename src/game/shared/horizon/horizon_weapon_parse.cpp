//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include <KeyValues.h>
#include "horizon_weapon_parse.h"


FileWeaponInfo_t* CreateWeaponInfo()
{
	return new CHorizonWeaponInfo;
}


CHorizonWeaponInfo::CHorizonWeaponInfo()
{
}

void CHorizonWeaponInfo::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );
}
