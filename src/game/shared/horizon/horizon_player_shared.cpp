#include "cbase.h"
#include "horizon_player_shared.h"
#ifdef CLIENT_DLL
#include "view.h"
#endif

const Vector& CHorizonPlayer::GetViewOffset() const
{
	return m_vecViewOffset;
}

void CHorizonPlayer::SetViewOffset(const Vector& v)
{
	m_vecViewOffset = v;
}

//-----------------------------------------------------------------------------
// Actual Eye position + angles
//-----------------------------------------------------------------------------
Vector CHorizonPlayer::EyePosition()
{
#ifdef CLIENT_DLL
	if (IsObserver())
	{
		if (m_iObserverMode == OBS_MODE_CHASE)
		{
			if (IsLocalPlayer(this))
			{
				return MainViewOrigin(GetSplitScreenPlayerSlot());
			}
		}
	}
#endif
	// if in camera mode, use that
	if (GetViewEntity() != NULL)
	{
		return GetViewEntity()->EyePosition();
	}

	return GetAbsOrigin() + m_vecViewOffset;
}

//-----------------------------------------------------------------------------
// Eye angles
//-----------------------------------------------------------------------------
const QAngle& CHorizonPlayer::EyeAngles()
{
	// NOTE: Viewangles are measured *relative* to the parent's coordinate system
	CBaseEntity* pMoveParent = GetMoveParent();

	// if in camera mode, use that
	if (GetViewEntity() != NULL)
	{
		return GetViewEntity()->EyeAngles();
	}

	// FIXME: Cache off the angles?
	matrix3x4_t eyesToParent, eyesToWorld;
	AngleMatrix(pl.v_angle, eyesToParent);
	if (pMoveParent)
		ConcatTransforms(pMoveParent->EntityToWorldTransform(), eyesToParent, eyesToParent);
	ConcatTransforms(m_matGravityEyesTransform.As3x4(), eyesToParent, eyesToWorld);

	static QAngle angEyeWorld;
	MatrixAngles(eyesToWorld, angEyeWorld);
	return angEyeWorld;
}