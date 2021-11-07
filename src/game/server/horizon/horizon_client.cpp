//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: HACB Client is the Game initialization point 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "horizon_player.h"
#include "horizon_gamerules.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "EntityList.h"
#include "physics.h"
#include "game.h"
#include "player_resource.h"
#include "engine/IEngineSound.h"
#include "viewport_panel_names.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void Host_Say( edict_t *pEdict, bool teamonly );

extern bool			g_fGameOver;

//ClientPutInServer - Called whenever a player is spawned in the game
void ClientPutInServer( edict_t *pEdict, const char *playername )
{
	// Allocate a CBasePlayer for pev, and call spawn
	CHorizonPlayer *pPlayer = CHorizonPlayer::CreatePlayer( "player", pEdict );
	pPlayer->SetPlayerName( playername );
	
	//Ratchet - Debugging/Checking code
	//char pointer PlayerName - Concatenate the Playername converted to a char pointer (Fuck knows why it wont take a constant), and append " has joined" to it
	char* LogPlayerName = strcat((char*)playername, " has joined the game\r\n");
	//Log it the the Source Engine console
	Msg(LogPlayerName, "\n");
}

void FinishClientPutInServer(CHorizonPlayer  *pPlayer)
{

	char sName[128];
	Q_strncpy(sName, pPlayer->GetPlayerName(), sizeof(sName));

	// First parse the name and remove any %'s
	for (char *pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++)
	{
		// Replace it with a space
		if (*pApersand == '%')
			*pApersand = ' ';
	}

	// notify other clients of player joining the game
	UTIL_ClientPrintAll(HUD_PRINTNOTIFY, "#Game_connected", sName[0] != 0 ? sName : "<unconnected>");

	const ConVar *hostname = cvar->FindVar("hostname");
	const char *title = (hostname) ? hostname->GetString() : "MESSAGE OF THE DAY";

	KeyValues *data = new KeyValues("data");
	data->SetString("title", title);		// info panel title
	data->SetString("type", "1");			// show userdata from stringtable entry
	data->SetString("msg", "motd");		// use this stringtable entry
		
	pPlayer->ShowViewPortPanel(PANEL_INFO, true, data);

	data->deleteThis();

}

//ClientActive - Called when the player connects and activates respectively. Defines the C++ class used for players
void ClientActive( edict_t *pEdict, bool bLoadGame )
{
	CHorizonPlayer *pPlayer = dynamic_cast< CHorizonPlayer* >( CBaseEntity::Instance( pEdict ) );
	Assert( pPlayer );

	if ( !pPlayer )
	{
		return;
	}

	pPlayer->InitialSpawn();

	if ( !bLoadGame )
	{
		pPlayer->Spawn();
		FinishClientPutInServer( pPlayer );
	}
}


void ClientFullyConnect( edict_t *pEntity )
{
}


/*
const char *GetGameDescription - Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
*/
const char *GetGameDescription()
{
	// this function may be called before the world has spawned, and the game rules initialized
	if (g_pGameRules)
		return g_pGameRules->GetGameDescription();
	else
		return "Horizon";
}

//-----------------------------------------------------------------------------
// Purpose: Given a player and optional name returns the entity of that 
//			classname that the player is nearest facing
//			
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity* FindEntity( edict_t *pEdict, char *classname)
{
	// If no name was given set bits based on the picked
	if (FStrEq(classname,"")) 
	{
		CBasePlayer *pPlayer = static_cast<CBasePlayer*>(GetContainingEntity(pEdict));
		if ( pPlayer )
		{
			return pPlayer->FindPickerEntityClass( classname );
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
PRECACHE_REGISTER_BEGIN( GLOBAL, ClientGamePrecache )
	PRECACHE( MODEL, "models/player.mdl");

	PRECACHE( KV_DEP_FILE, "resource/ParticleEmitters.txt" )
PRECACHE_REGISTER_END()

void ClientGamePrecache( void )
{
}

// called by ClientKill and DeadThink
void respawn( CBaseEntity *pEdict, bool fCopyCorpse )
{
	//Should never actually use these as we're using SP and not coop, but its really handy for debugging.
	//Infact now that I think about it, creating check-points would be handy/useful.
	//Check points as custom brushes, on overlap set "Spawn Position" to the hit position.
	//Whats happening here is it just reloading the map
	if (gpGlobals->coop || gpGlobals->deathmatch)
	{
		if ( fCopyCorpse )
		{
			// make a copy of the dead body for appearances sake
			((CHorizonPlayer *)pEdict)->CreateCorpse();
		}

		// respawn player
		pEdict->Spawn();
	}
	else
	{       // restart the entire server
		engine->ServerCommand("reload\n");
	}
}

void GameStartFrame( void )
{
	VPROF("GameStartFrame()");
	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = (teamplay.GetInt() != 0);

}


//=========================================================
// instantiate the proper game rules object
//=========================================================
void InstallGameRules()
{
	CreateGameRulesObject( "CHorizonGameRules" );
}

