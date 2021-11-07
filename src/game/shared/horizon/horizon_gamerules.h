#ifndef HORIZON_GAMERULES_H
#define HORIZON_GAMERULES_H

#include "teamplay_gamerules.h"

#ifdef CLIENT_DLL
	#define CHorizonGameRules C_HorizonGameRules
	#define CHorizonGameRulesProxy C_HorizonGameRulesProxy
#endif

class CHorizonGameRulesProxy : public CGameRulesProxy
{
	DECLARE_CLASS(CHorizonGameRulesProxy, CGameRulesProxy);
	DECLARE_NETWORKCLASS();
public:
};

class CHorizonGameRules : public CTeamplayRules
{
	DECLARE_CLASS(CHorizonGameRules, CTeamplayRules);
	DECLARE_NETWORKCLASS_NOBASE();
public:

	CHorizonGameRules();
	virtual ~CHorizonGameRules();

	inline const float GetDuckingViewOffset() const { return 28; }
	inline const float GetStandingViewOffset() const { return 64; }

	virtual const CViewVectors* GetViewVectors() const;
	virtual CViewVectors* GetViewVectors();

	virtual Vector DefaultGravityDirection() { return Vector(0, 0, -1); }

#ifdef GAME_DLL
	virtual void Think();
#endif
};

inline CHorizonGameRules* HorizonGameRules()
{
	return static_cast<CHorizonGameRules*>(g_pGameRules);
}

inline CViewVectors* GetViewVectors()
{
	return HorizonGameRules()->GetViewVectors();
}

#endif // HORIZON_GAMERULES_H
