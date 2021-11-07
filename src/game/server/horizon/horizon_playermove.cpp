//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "player_command.h"
#include "igamemovement.h"
#include "in_buttons.h"
#include "ipredictionsystem.h"
#include "horizon_player.h"
#include "horizon_movedata.h"

static CHorizonMoveData g_MoveData;
CMoveData *g_pMoveData = &g_MoveData;

IPredictionSystem *IPredictionSystem::g_pPredictionSystems = NULL;

extern IGameMovement *g_pGameMovement;
extern ConVar sv_noclipduringpause;

//-----------------------------------------------------------------------------
// Sets up the move data for Infested
//-----------------------------------------------------------------------------
class CHorizonPlayerMove : public CPlayerMove
{
DECLARE_CLASS( CHorizonPlayerMove, CPlayerMove );

public:
	virtual void	SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void	RunCommand( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *moveHelper );
};

// PlayerMove Interface
static CHorizonPlayerMove g_PlayerMove;

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
CPlayerMove *PlayerMove()
{
	return &g_PlayerMove;
}

//-----------------------------------------------------------------------------
// Purpose: This is called pre player movement and copies all the data necessary
//          from the player for movement. (Server-side, the client-side version
//          of this code can be found in prediction.cpp.)
//-----------------------------------------------------------------------------
void CHorizonPlayerMove::SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	//player->AvoidPhysicsProps( ucmd );

	BaseClass::SetupMove( player, ucmd, pHelper, move );

	// setup trace optimization
	g_pGameMovement->SetupMovementBounds( move );
}

void CHorizonPlayerMove::RunCommand( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *moveHelper )
{
	BaseClass::RunCommand( player, ucmd, moveHelper );
}
