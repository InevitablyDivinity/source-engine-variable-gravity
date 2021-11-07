#include "cbase.h"
#include "horizon_gamerules.h"
#include "ammodef.h"

#ifndef CLIENT_DLL
	#include "voice_gamemgr.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

REGISTER_GAMERULES_CLASS( CHorizonGameRules );

BEGIN_NETWORK_TABLE_NOBASE( CHorizonGameRules, DT_HorizonGameRules )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( horizon_gamerules, CHorizonGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( HorizonGameRulesProxy, DT_HorizonGameRulesProxy )

#ifdef CLIENT_DLL
	void RecvProxy_HorizonGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CHorizonGameRules *pRules = HorizonGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CHorizonGameRulesProxy, DT_HorizonGameRulesProxy )
		RecvPropDataTable( "horizon_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_HorizonGameRules ), RecvProxy_HorizonGameRules )
	END_RECV_TABLE()
#else
	void *SendProxy_HorizonGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CHorizonGameRules *pRules = HorizonGameRules();
		Assert( pRules );
		pRecipients->SetAllRecipients();
		return pRules;
	}

	BEGIN_SEND_TABLE( CHorizonGameRulesProxy, DT_HorizonGameRulesProxy )
		SendPropDataTable( "horizon_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_HorizonGameRules ), SendProxy_HorizonGameRules )
	END_SEND_TABLE()
#endif

CAmmoDef* GetAmmoDef()
{
	static CAmmoDef def;
	static bool bInitted = false;

	if (!bInitted)
	{
		bInitted = true;

		def.AddAmmoType("shells", DMG_BUCKSHOT, TRACER_NONE, 0, 0, 200/*max carry*/, 1, 0);

	}

	return &def;
}

#ifndef CLIENT_DLL
	// --------------------------------------------------------------------------------------------------- //
	// Voice helper
	// --------------------------------------------------------------------------------------------------- //

	class CVoiceGameMgrHelper : public IVoiceGameMgrHelper
	{
	public:
		virtual bool		CanPlayerHearPlayer(CBasePlayer *pListener, CBasePlayer *pTalker, bool &bProximity)
		{
			// players can always hear each other in Infested
			return true;
		}
	};
	CVoiceGameMgrHelper g_VoiceGameMgrHelper;
	IVoiceGameMgrHelper *g_pVoiceGameMgrHelper = &g_VoiceGameMgrHelper;

	// World.cpp calls this but we don't use it in DML.
	void InitBodyQue()
	{
	}
#endif

CHorizonGameRules::CHorizonGameRules()
{
}
	
CHorizonGameRules::~CHorizonGameRules()
{
}

#ifdef CLIENT_DLL
#else
void CHorizonGameRules::Think()
{
	BaseClass::Think();
}
#endif

static CViewVectors g_DefaultViewVectors(
	Vector(0, 0, 64),		// VEC_VIEW				(m_vView)

	Vector(-16, -16, 0),	// VEC_HULL_MIN			(m_vHullMin)
	Vector(16, 16, 72),		// VEC_HULL_MAX			(m_vHullMax)

	Vector(-16, -16, 0),	// VEC_DUCK_HULL_MIN	(m_vDuckHullMin)
	Vector(16, 16, 36),		// VEC_DUCK_HULL_MAX	(m_vDuckHullMax)
	Vector(0, 0, 28),		// VEC_DUCK_VIEW		(m_vDuckView)

	Vector(-10, -10, -10),	// VEC_OBS_HULL_MIN		(m_vObsHullMin)
	Vector(10, 10, 10),		// VEC_OBS_HULL_MAX		(m_vObsHullMax)

	Vector(0, 0, 14)		// VEC_DEAD_VIEWHEIGHT	(m_vDeadViewHeight)
);

const CViewVectors* CHorizonGameRules::GetViewVectors() const
{
	return &g_DefaultViewVectors;
}

CViewVectors* CHorizonGameRules::GetViewVectors()
{
	return &g_DefaultViewVectors;
}