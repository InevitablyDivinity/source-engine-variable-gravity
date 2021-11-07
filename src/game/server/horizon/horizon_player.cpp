#include "cbase.h"
#include "horizon_player.h"
#include "predicted_viewmodel.h"
#include "horizon_gamerules.h"
#include "horizon_gamemovement.h"
#include "gravity_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IGameMovement* g_pGameMovement;

ConVar player_gravity_camera_rotate_time("player_gravity_camera_rotate_time", "0.7", FCVAR_CHEAT);

static void ChangeGravityDirectionCallback(const CCommand& args)
{
	Vector vecGravityDirection;
	char szArg[30];
	V_snprintf(szArg, 30, "%s %s %s", args.Arg(1), args.Arg(2), args.Arg(3));
	UTIL_StringToVector(vecGravityDirection.Base(), szArg);

	CHorizonPlayer* pPlayer = (CHorizonPlayer*)UTIL_GetCommandClient();
	pPlayer->SetGravityDirection(vecGravityDirection);
}
ConCommand set_gravity_dir("set_gravity_dir", ChangeGravityDirectionCallback, "Sets your direction of gravity.", FCVAR_CHEAT);

class CTEPlayerAnimEvent : public CBaseTempEntity
{
public:
	DECLARE_CLASS(CTEPlayerAnimEvent, CBaseTempEntity);
	DECLARE_SERVERCLASS();

	CTEPlayerAnimEvent(const char *name) : CBaseTempEntity(name)
	{
	}

	CNetworkHandle(CBasePlayer, m_hPlayer);
	CNetworkVar(int, m_iEvent);
};

IMPLEMENT_SERVERCLASS_ST_NOBASE(CTEPlayerAnimEvent, DT_TEPlayerAnimEvent)
	SendPropEHandle(SENDINFO(m_hPlayer)),
	SendPropInt(SENDINFO(m_iEvent), Q_log2(PLAYERANIMEVENT_COUNT) + 1, SPROP_UNSIGNED),
END_SEND_TABLE()

static CTEPlayerAnimEvent g_TEPlayerAnimEvent("PlayerAnimEvent");

void TE_PlayerAnimEvent(CBasePlayer *pPlayer, PlayerAnimEvent_t event, bool bPredicted)
{
	CPVSFilter filter((const Vector&)pPlayer->EyePosition());
	if (bPredicted)
		filter.UsePredictionRules();

	// The player himself doesn't need to be sent his animation events 
	// unless cs_showanimstate wants to show them.
	//if ( asw_showanimstate.GetInt() == pPlayer->entindex() )
	//{
		//filter.RemoveRecipient( pPlayer );
	//}

	g_TEPlayerAnimEvent.m_hPlayer = pPlayer;
	g_TEPlayerAnimEvent.m_iEvent = event;
	g_TEPlayerAnimEvent.Create(filter, 0);
}

LINK_ENTITY_TO_CLASS(player, CHorizonPlayer);
PRECACHE_REGISTER(player);

IMPLEMENT_SERVERCLASS_ST(CHorizonPlayer, DT_HorizonPlayer)
	SendPropVector(SENDINFO(m_angGravityEyes)),
	SendPropVector(SENDINFO(m_vecGravityDir)),
	SendPropVector(SENDINFO(m_vecViewOffset)),
END_SEND_TABLE()

BEGIN_DATADESC(CHorizonPlayer)
END_DATADESC()

CHorizonPlayer::CHorizonPlayer()
{
	m_pPlayerAnimState = CreateHorizonPlayerAnimState( this );
	UseClientSideAnimation();
	m_angEyeAngles.Init();

	m_angGravityEyes.Init();
	m_vecGravityDir.Init();
	m_vecViewOffset.Init();
	m_vecGravityAxis.Init();
	m_vecGravityAxisInverse.Init();
}

CHorizonPlayer::~CHorizonPlayer()
{
	m_pPlayerAnimState->Release();
}

void CHorizonPlayer::InitialSpawn()
{
	BaseClass::InitialSpawn();
	m_bIsFirstSpawn = true;
}

void CHorizonPlayer::Spawn()
{
	BaseClass::Spawn();

	Precache();

	SetModel("models/player/chell/player.mdl");

	GiveNamedItem("weapon_gravity");

	MatrixSetIdentity(m_matGravityEyesTransform);
	MatrixSetIdentity(m_matGravityTransform);
	m_angGravityEyes = vec3_angle;
	m_bRotateCamera = false;
	m_vecGravityDir = HorizonGameRules()->DefaultGravityDirection();
	GetGravityAxes(m_vecGravityDir, m_vecGravityAxis, m_vecGravityAxisInverse);


	/*if (m_bIsFirstSpawn)
	{
		SetGravityDirection(HorizonGameRules()->DefaultGravityDirection(), false);
		m_bIsFirstSpawn = false;
	}
	else
	{
		SetGravityDirection(HorizonGameRules()->DefaultGravityDirection(), true);
	}*/
}

void CHorizonPlayer::Precache()
{
	BaseClass::Precache();
	PrecacheModel("models/player/chell/player.mdl");
}

int CHorizonPlayer::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState(FL_EDICT_ALWAYS);
}

CHorizonPlayer *CHorizonPlayer::CreatePlayer( const char *className, edict_t *ed )
{
	CHorizonPlayer::s_PlayerEdict = ed;
	return static_cast<CHorizonPlayer*>(CreateEntityByName( className ));
}

void CHorizonPlayer::DoAnimationEvent(PlayerAnimEvent_t event, bool bPredicted)
{
	TE_PlayerAnimEvent(this, event, bPredicted);	// Send to any clients who can see this guy.
	m_pPlayerAnimState->DoAnimationEvent(event);
}

void CHorizonPlayer::PostThink()
{
	BaseClass::PostThink();

	if (m_bRotateCamera)
		HandleCameraRotation();

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles(angles);

	m_pPlayerAnimState->Update(LocalEyeAngles()[YAW], LocalEyeAngles()[PITCH]);

	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();
}

void CHorizonPlayer::HandleCameraRotation()
{
	float fraction = (gpGlobals->curtime - m_flGravityLerpStart) / player_gravity_camera_rotate_time.GetFloat();
	if (fraction > 1)
	{
		m_bRotateCamera = false;
		return;
	}
	else
	{
		Quaternion q;
		QuaternionSlerp(m_quaRotateCameraFrom, m_quaRotateCameraTo, fraction, q);

		QAngle& angles = m_angGravityEyes.GetForModify();
		QuaternionAngles(q, angles);
		AngleMatrix(angles, m_matGravityEyesTransform.As3x4());
	}
}

void CHorizonPlayer::SetGravityDirection(Vector vecGravityDir)
{
	// Create rotation matrix for eye angles
	Vector vecAxisOfRotation = CrossProduct(m_vecGravityDir.Get(), vecGravityDir);
	float fAngleOfRotation = RAD2DEG(acos(DotProduct(m_vecGravityDir.Get(), vecGravityDir)));

	VMatrix mat;
	MatrixBuildRotationAboutAxis(mat, vecAxisOfRotation, fAngleOfRotation);

	QAngle eyeAngles = EyeAngles();
	AngleQuaternion(eyeAngles, m_quaRotateCameraFrom);
	// Convert m_quaRotateCameraFrom to local eye angles

	Vector vecForward, vecUp;
	AngleVectors(eyeAngles, &vecForward, nullptr, &vecUp);
	VectorRotate(vecForward, mat.As3x4(), vecForward);
	VectorRotate(vecUp, mat.As3x4(), vecUp);
	VectorAngles(vecForward, vecUp, eyeAngles);

	AngleQuaternion(eyeAngles, m_quaRotateCameraTo);
	// Convert m_quaRotateCameraTo to local eye angles

#if 0 // no lerp
	m_angGravityEyes = m_angGravityEyesDesiredTo;
	AngleMatrix(m_angGravityEyesDesiredTo, m_matGravityEyesTransform.As3x4());
#else // lerp
	m_flGravityLerpStart = gpGlobals->curtime;
	m_bRotateCamera = true;
#endif

	// Create rotation matrix for everything else
	MatrixBuildRotation(m_matGravityTransform, m_vecGravityDir, vecGravityDir);

	// Rotate the origin about the view offset
	Vector vecNewOrigin = GetAbsOrigin() - EyePosition();
	VectorRotate(vecNewOrigin, m_matGravityTransform.As3x4(), vecNewOrigin);
	SetAbsOrigin(vecNewOrigin + EyePosition());

	// Rotate the view offset
	Vector vecViewOffset = GetViewOffset();
	VectorRotate(vecViewOffset, m_matGravityTransform.As3x4(), vecViewOffset);
	SetViewOffset(vecViewOffset);

	// Rotate the bounding box
	RotateAABB(m_matGravityTransform.As3x4(),
		GetViewVectors()->m_vDuckHullMin, GetViewVectors()->m_vDuckHullMax,
		GetViewVectors()->m_vDuckHullMin, GetViewVectors()->m_vDuckHullMax);
	RotateAABB(m_matGravityTransform.As3x4(),
		GetViewVectors()->m_vHullMin, GetViewVectors()->m_vHullMax,
		GetViewVectors()->m_vHullMin, GetViewVectors()->m_vHullMax);

	// Rotate the model
	/*Vector vecForward, vecUp;
	AngleVectors(m_pPlayerAnimState->GetRenderAngles(), &vecForward, NULL, &vecUp);
	VectorRotate(vecForward, m_matGravityTransform.As3x4(), vecForward);
	VectorRotate(vecUp, m_matGravityTransform.As3x4(), vecUp);

	QAngle angRenderAngles;
	VectorAngles(vecForward, vecUp, angRenderAngles);
	m_pPlayerAnimState->SetRenderAngles(angRenderAngles);*/

	m_vecGravityDir = vecGravityDir;
	GetGravityAxes(m_vecGravityDir, m_vecGravityAxis, m_vecGravityAxisInverse);
}

void CHorizonPlayer::CreateViewModel(int index /*=0*/)
{
	Assert(index >= 0 && index < MAX_VIEWMODELS);

	if (GetViewModel(index))
		return;

	CPredictedViewModel* pViewModel = static_cast<CPredictedViewModel*>(CreateEntityByName("predicted_viewmodel"));
	if (pViewModel)
	{
		pViewModel->SetAbsOrigin(GetAbsOrigin());
		pViewModel->SetOwner(this);
		pViewModel->SetIndex(index);
		DispatchSpawn(pViewModel);
		pViewModel->FollowEntity(this, false);
		m_hViewModel.Set(index, pViewModel);
	}
}