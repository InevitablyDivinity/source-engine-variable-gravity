#ifndef C_HORIZON_PLAYER_H
#define C_HORIZON_PLAYER_H

#ifdef _WIN32
#pragma once
#endif

#include "c_baseplayer.h"
#include "horizon_playeranimstate.h"

class C_HorizonPlayer : public C_BasePlayer
{
	DECLARE_CLASS(C_HorizonPlayer, C_BasePlayer);
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_INTERPOLATION();
public:

	C_HorizonPlayer();
	virtual ~C_HorizonPlayer();

	virtual void Spawn();

	virtual bool ShouldRegenerateOriginFromCellBits() const;

	virtual Vector EyePosition();
	virtual const QAngle& EyeAngles(void);
	virtual const QAngle& GetRenderAngles();

	virtual void UpdateClientSideAnimation();

	virtual void PostDataUpdate(DataUpdateType_t updateType);
	virtual void OnDataChanged(DataUpdateType_t updateType);

	void DoAnimationEvent(PlayerAnimEvent_t event, bool bPredicted = false);

	virtual const Vector& GetViewOffset() const;
	virtual void SetViewOffset(const Vector& v);

	inline Vector GetGravityDirection() const { return m_vecGravityDir; }
	inline Vector GetGravityAxis() const { return m_vecGravityAxis; }
	inline Vector GetGravityAxisInverse() const { return m_vecGravityAxisInverse; }
	inline matrix3x4_t GetUpdateGravityTransform() const { return m_matGravityTransform.As3x4(); }

private:
	CHorizonPlayerAnimState* m_pPlayerAnimState;
	QAngle	m_angEyeAngles;
	CInterpolatedVar< QAngle >	m_iv_angEyeAngles;

	Vector m_vecGravityDir;
	Vector m_vecGravityAxis;
	Vector m_vecGravityAxisInverse;
	Vector m_vecViewOffset;

	VMatrix m_matGravityTransform;
	VMatrix m_matGravityEyesTransform;

	QAngle m_angGravityEyes;

	C_HorizonPlayer(const C_HorizonPlayer &);
};

inline C_HorizonPlayer* ToHorizonPlayer( CBaseEntity *pPlayer )
{
	Assert( dynamic_cast<C_HorizonPlayer*>( pPlayer ) != NULL );
	return static_cast<C_HorizonPlayer*>( pPlayer );
}

#endif // C_HORIZON_PLAYER_H
