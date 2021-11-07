#ifndef HORIZON_PLAYER_H
#define HORIZON_PLAYER_H

#include "basemultiplayerplayer.h"
#include "horizon_playeranimstate.h"

class CHorizonPlayer : public CBaseMultiplayerPlayer
{
	DECLARE_CLASS(CHorizonPlayer, CBaseMultiplayerPlayer);
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
public:

	static CHorizonPlayer* CreatePlayer(const char* className, edict_t* ed);

	CHorizonPlayer();
	virtual ~CHorizonPlayer();

	virtual void InitialSpawn();
	virtual void Spawn();

	virtual void Precache();

	virtual Vector EyePosition();

	virtual int UpdateTransmitState();

	virtual void PostThink();

	virtual const QAngle& EyeAngles();

	virtual void CreateViewModel(int viewmodelindex = 0);

//	virtual void SetMoveType(MoveType_t moveType, MoveCollide_t moveCollide = MOVECOLLIDE_DEFAULT);

	virtual const Vector& GetViewOffset() const;
	virtual void SetViewOffset(const Vector& v);

	// This passes the event to the client's and server's CPlayerAnimState.
	void DoAnimationEvent(PlayerAnimEvent_t event, bool bPredicted = false);

	// Animstate handles this.
	void SetAnimation(PLAYER_ANIM playerAnim) { return; }

	void HandleCameraRotation();
	void SetGravityVariables(Vector vecGravityDir);
	void SetGravityDirection(Vector vecGravityDir);

	inline Vector GetGravityDirection() const { return m_vecGravityDir.Get(); }
	inline Vector GetGravityAxis() const { return m_vecGravityAxis; }
	inline Vector GetGravityAxisInverse() const { return m_vecGravityAxisInverse; }
	inline matrix3x4_t GetUpdateGravityTransform() const { return m_matGravityTransform.As3x4(); }

private:
	bool m_bIsFirstSpawn;
	bool m_bRotateCamera;
	
	VMatrix m_matGravityTransform;
	VMatrix m_matGravityEyesTransform;
	CNetworkQAngle(m_angGravityEyes);

	float m_flGravityLerpStart;
	// To lerp between new eye angles
	Quaternion m_quaRotateCameraTo;
	Quaternion m_quaRotateCameraFrom;

	// Used for when we switch MOVETYPE to something like noclip,
	// save the gravity direction so we can re-orient the upwards
	Vector m_vecInactiveGravityDir;

	CNetworkVector(m_vecGravityDir);
	Vector m_vecGravityAxis;
	Vector m_vecGravityAxisInverse;
	CNetworkVector(m_vecViewOffset);

	CNetworkQAngle(m_angEyeAngles);	// Copied from EyeAngles() so we can send it to the client.
	CHorizonPlayerAnimState *m_pPlayerAnimState;
};


inline CHorizonPlayer *ToHorizonPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return nullptr;

#ifdef _DEBUG
	Assert( dynamic_cast<CHorizonPlayer *>( pEntity ) != 0 );
#endif
	return static_cast<CHorizonPlayer *>( pEntity );
}


#endif	// HORIZON_PLAYER_H
