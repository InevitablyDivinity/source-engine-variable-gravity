#include "cbase.h"
#include "clientmode_horizon.h"
#include "horizon_loading_panel.h"
#include "c_basetempentity.h"
#include "c_horizon_player.h"
#include "horizon_gamerules.h"
#include "gravity_shared.h"

#if defined( CHorizonPlayer )
	#undef CHorizonPlayer
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class C_TEPlayerAnimEvent : public C_BaseTempEntity
{
	DECLARE_CLASS(C_TEPlayerAnimEvent, C_BaseTempEntity);
	DECLARE_CLIENTCLASS();
public:

	virtual void PostDataUpdate( DataUpdateType_t updateType )
	{
		// Create the effect.
		C_HorizonPlayer *pPlayer = dynamic_cast< C_HorizonPlayer* >( m_hPlayer.Get() );
		if ( pPlayer && !pPlayer->IsDormant() )
		{
			pPlayer->DoAnimationEvent( (PlayerAnimEvent_t)m_iEvent.Get() );
		}	
	}

public:
	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
};

IMPLEMENT_CLIENTCLASS_EVENT( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent, CTEPlayerAnimEvent );

BEGIN_RECV_TABLE_NOBASE( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
	RecvPropInt( RECVINFO( m_iEvent ) )
END_RECV_TABLE()

BEGIN_RECV_TABLE_NOBASE( C_HorizonPlayer, DT_HorizonLocalPlayerExclusive )
	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
//	RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),
END_RECV_TABLE()

BEGIN_RECV_TABLE_NOBASE(C_HorizonPlayer, DT_HorizonNonLocalPlayerExclusive)
	RecvPropVector(RECVINFO_NAME(m_vecNetworkOrigin, m_vecOrigin)),
	RecvPropFloat(RECVINFO(m_angEyeAngles[0])),
	RecvPropFloat(RECVINFO(m_angEyeAngles[1])),
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT(C_HorizonPlayer, DT_HorizonPlayer, CHorizonPlayer)
	RecvPropDataTable( "horizonlocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_HorizonLocalPlayerExclusive) ),
	RecvPropDataTable( "horizonnonlocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_HorizonNonLocalPlayerExclusive) ),
	RecvPropVector(RECVINFO(m_angGravityEyes)),
	RecvPropVector(RECVINFO(m_vecGravityDir)),
	RecvPropVector(RECVINFO(m_vecViewOffset)),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_HorizonPlayer )
	//DEFINE_PRED_FIELD( m_vecVelocity, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	//DEFINE_PRED_FIELD( m_flAnimTime, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),	
	DEFINE_PRED_FIELD( m_nSequence, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),	
	DEFINE_PRED_FIELD( m_nNewSequenceParity, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_nResetEventsParity, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	//DEFINE_PRED_FIELD( m_iEFlags, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()


C_HorizonPlayer::C_HorizonPlayer()
	: m_iv_angEyeAngles( "C_HorizonPlayer::m_iv_angEyeAngles" )
{
	m_pPlayerAnimState = CreateHorizonPlayerAnimState( this );
	SetPredictionEligible( true );

	m_angEyeAngles.Init();
	AddVar( &m_angEyeAngles, &m_iv_angEyeAngles, LATCH_SIMULATION_VAR );
}

C_HorizonPlayer::~C_HorizonPlayer() 
{
	m_pPlayerAnimState->Release();
}

void C_HorizonPlayer::Spawn()
{
	BaseClass::Spawn();
}

bool C_HorizonPlayer::ShouldRegenerateOriginFromCellBits() const
{
	return true;
}

const QAngle& C_HorizonPlayer::GetRenderAngles()
{
	if ( IsRagdoll() )
		return vec3_angle;
	else
		return m_pPlayerAnimState->GetRenderAngles();
}

void C_HorizonPlayer::UpdateClientSideAnimation()
{
	m_pPlayerAnimState->Update( EyeAngles()[YAW], EyeAngles()[PITCH] );
	BaseClass::UpdateClientSideAnimation();
}

void C_HorizonPlayer::DoAnimationEvent( PlayerAnimEvent_t event, bool bPredicted )
{
	m_pPlayerAnimState->DoAnimationEvent( event );
}

void C_HorizonPlayer::PostDataUpdate( DataUpdateType_t updateType )
{
	// C_BaseEntity assumes we're networking the entity's angles, so pretend that it
	// networked the same value we already have.
	SetNetworkAngles( GetLocalAngles() );
	
	BaseClass::PostDataUpdate( updateType );
}

void C_HorizonPlayer::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	// only update for m_angLocalToGravityTransform
	VMatrix matLocalToGravityTransform;
	AngleMatrix(m_angGravityEyes, m_matGravityEyesTransform.As3x4());
	// construct manually

	// use proxy so only updates when m_vecGravityDir changes
	GetGravityAxes(m_vecGravityDir, m_vecGravityAxis, m_vecGravityAxisInverse);

	QAngle angAngles;
	VectorAngles(Forward() * GetGravityAxisInverse(), angAngles);
	m_pPlayerAnimState->SetRenderAngles(angAngles); // Rotate player model angles for gravity

	UpdateVisibility();
}