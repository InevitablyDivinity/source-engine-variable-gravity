#ifndef _INCLUDED_CLIENTMODE_HORIZON_H
#define _INCLUDED_CLIENTMODE_HORIZON_H
#ifdef _WIN32
#pragma once
#endif

#include "clientmode_shared.h"
#include <vgui_controls/EditablePanel.h>
#include <vgui/Cursor.h>
#include "GameUI/igameui.h"

class CHudViewport;

class ClientModeHorizon : public ClientModeShared
{
public:
	DECLARE_CLASS( ClientModeHorizon, ClientModeShared );

	virtual void	Init();
	virtual void	InitViewport();
	virtual void	Shutdown();

	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );
	virtual void	FireGameEvent( IGameEvent *event );
	virtual void	DoPostScreenSpaceEffects( const CViewSetup *pSetup );
	virtual void	Horizon_CloseAllWindows();
	virtual void	Horizon_CloseAllWindowsFrom(vgui::Panel* pPanel);
};

extern IClientMode *GetClientModeNormal();
extern vgui::HScheme g_hVGuiCombineScheme;

extern ClientModeHorizon* GetClientModeHorizon();

#endif // _INCLUDED_CLIENTMODE_HORIZON_H
