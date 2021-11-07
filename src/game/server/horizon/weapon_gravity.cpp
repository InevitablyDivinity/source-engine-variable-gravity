#include "cbase.h"
#include "weapon_horizonbase.h"
#include "in_buttons.h"

class CWeaponGravity : public CWeaponHorizonBase
{
	DECLARE_CLASS(CWeaponGravity, CWeaponHorizonBase);
public:
	virtual void PrimaryAttack();
};

LINK_ENTITY_TO_CLASS(weapon_gravity, CWeaponGravity);

void CWeaponGravity::PrimaryAttack()
{
	CBasePlayer* pOwner = (CBasePlayer*)GetOwner();

	Vector vecDir;
	AngleVectors(pOwner->EyeAngles(), &vecDir);

	Vector vecStart, vecEnd;
	vecStart = pOwner->EyePosition();
	vecEnd = vecStart + (vecDir * MAX_TRACE_LENGTH);

	CTraceFilterWorldOnly traceFilter;

	trace_t tr;
	UTIL_TraceLine(vecStart, vecEnd, MASK_SOLID, &traceFilter, &tr);

	if (tr.m_pEnt && tr.m_pEnt->IsWorld())
	{
		Vector normal = tr.plane.normal;
		engine->ClientCommand(pOwner->edict(), "set_gravity_dir %f %f %f", -normal.x, -normal.y, -normal.z);
	}

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
}