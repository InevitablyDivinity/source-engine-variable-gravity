//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_HORIZONBASE_H
#define WEAPON_HORIZONBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "horizon_playeranimstate.h"
#include "horizon_weapon_parse.h"
#include "horizon_shareddefs.h"

#if defined( CLIENT_DLL )
	#define CWeaponHorizonBase C_WeaponHorizonBase
#endif

// These are the names of the ammo types that the weapon script files reference.
class CWeaponHorizonBase : public CBaseCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponHorizonBase, CBaseCombatWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponHorizonBase();

	#ifdef GAME_DLL
		DECLARE_DATADESC();
	#endif
	#ifdef CLIENT_DLL
       virtual bool ShouldPredict();
	#endif
	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted() const { return true; }

	// Get DML weapon specific weapon data.
	CHorizonWeaponInfo const	&GetHorizonWpnData() const;

	// Get a pointer to the player that owns this weapon
	CHorizonPlayer* GetPlayerOwner() const;

	// Default weapon spread, pretty accurate - accuracy systems would need to modify this
	virtual const Vector& GetBulletSpread(void);

	virtual void PrimaryAttack();
	virtual void SecondaryAttack();

	//Tony; these five functions return the sequences the view model uses for a particular action. -- You can override any of these in a particular weapon if you want them to do
	//something different, ie: when a pistol is out of ammo, it would show a different sequence.
	virtual Activity	GetIdleActivity( void ) { return ACT_VM_IDLE; }
	virtual Activity	GetDeployActivity( void ) { return ACT_VM_DRAW; }
	virtual Activity	GetReloadActivity( void ) { return ACT_VM_RELOAD; }
	virtual Activity	GetHolsterActivity( void ) { return ACT_VM_HOLSTER; }

	virtual void			AddViewKick(void);

	
private:
	CWeaponHorizonBase( const CWeaponHorizonBase & );


};


#endif // WEAPON_HORIZONBASE_H
