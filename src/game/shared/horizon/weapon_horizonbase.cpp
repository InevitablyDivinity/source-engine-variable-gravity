//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "weapon_horizonbase.h"
#include "ammodef.h"
#include "datacache/imdlcache.h"

#if defined( CLIENT_DLL )

	#include "c_horizon_player.h"

#else

	#include "horizon_player.h"

#endif

// ----------------------------------------------------------------------------- //
// CWeaponHorizonBase tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponHorizonBase, DT_WeaponHorizonBase )

BEGIN_NETWORK_TABLE( CWeaponHorizonBase, DT_WeaponHorizonBase )
#ifdef CLIENT_DLL
#else
	SendPropExclude( "DT_BaseAnimating", "m_nNewSequenceParity" ),
	SendPropExclude( "DT_BaseAnimating", "m_nResetEventsParity" ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponHorizonBase )
END_PREDICTION_DATA()
#endif

#ifdef GAME_DLL

	BEGIN_DATADESC( CWeaponHorizonBase )

		// New weapon Think and Touch Functions go here..

	END_DATADESC()

#endif

#ifdef CLIENT_DLL
bool CWeaponHorizonBase::ShouldPredict()
{
       if ( GetOwner() && GetOwner() == C_BasePlayer::GetLocalPlayer())
               return true;

       return BaseClass::ShouldPredict();
}
#endif
// ----------------------------------------------------------------------------- //
// CWeaponHorizonBase implementation. 
// ----------------------------------------------------------------------------- //
CWeaponHorizonBase::CWeaponHorizonBase()
{
	SetPredictionEligible( true );

	AddSolidFlags( FSOLID_TRIGGER ); // Nothing collides with these but it gets touches.
}

const CHorizonWeaponInfo &CWeaponHorizonBase::GetHorizonWpnData() const
{
	const FileWeaponInfo_t *pWeaponInfo = &GetWpnData();
	const CHorizonWeaponInfo *pHorizonInfo;

	#ifdef _DEBUG
		pHorizonInfo = dynamic_cast< const CHorizonWeaponInfo* >( pWeaponInfo );
		Assert(pHorizonInfo);
	#else
		pHorizonInfo = static_cast< const CHorizonWeaponInfo* >( pWeaponInfo );
	#endif

	return *pHorizonInfo;
}

CHorizonPlayer* CWeaponHorizonBase::GetPlayerOwner() const
{
	return dynamic_cast< CHorizonPlayer* >( GetOwner() );
}

const Vector & CWeaponHorizonBase::GetBulletSpread(void)
{
	static Vector spread(0.01f, 0.01f, 0.0f);
	return spread;
}

void CWeaponHorizonBase::PrimaryAttack( void )
{
	
	CHorizonPlayer *pPlayer = GetPlayerOwner();

	if (!pPlayer)
		return;

	if (pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		return;

	pPlayer->RemoveAmmo(1, m_iPrimaryAmmoType);

	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( GetPrimaryAttackActivity() );
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY, true );

	Vector vecSrc = pPlayer->Weapon_ShootPosition();

	Vector vecAiming = pPlayer->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);

	FireBulletsInfo_t info(1, vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType);
	info.m_pAttacker = pPlayer;
	info.m_iTracerFreq = 2;
	info.m_flDamage = 10;

	pPlayer->FireBullets(info);


	//Add our view kick in
	AddViewKick();

	//Tony; update our weapon idle time
	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
}

void CWeaponHorizonBase::SecondaryAttack()
{
}

void CWeaponHorizonBase::AddViewKick(void)
{
	CHorizonPlayer *pPlayer = GetPlayerOwner();
	if (!pPlayer)
		return;

	QAngle punch(-1,0,0);
	pPlayer->ViewPunch(punch);
	
}
