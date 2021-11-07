#include "cbase.h"
#include "clientmode_horizon.h"
#include "vgui_int.h"
#include "ienginevgui.h"
#include "cdll_client_int.h"
#include "engine/IEngineSound.h"

#include "horizon_loading_panel.h"
#include "horizon_logo_panel.h"
#include "ivmodemanager.h"
#include "panelmetaclassmgr.h"
#include "nb_header_footer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar default_fov( "default_fov", "75", FCVAR_CHEAT );
ConVar fov_desired( "fov_desired", "75", FCVAR_USERINFO, "Sets the base field-of-view.", true, 1.0, true, 75.0 );

vgui::HScheme g_hVGuiCombineScheme = 0;

vgui::DHANDLE<CHorizon_Logo_Panel> g_hLogoPanel;

static IClientMode *g_pClientMode[ MAX_SPLITSCREEN_PLAYERS ];
IClientMode *GetClientMode()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return g_pClientMode[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

// --------------------------------------------------------------------------------- //
// CASWModeManager.
// --------------------------------------------------------------------------------- //

class CHorizonModeManager : public IVModeManager
{
public:
	virtual void	Init();
	virtual void	SwitchMode( bool commander, bool force ) {}
	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );
	virtual void	ActivateMouse( bool isactive ) {}
};

static CHorizonModeManager g_ModeManager;
IVModeManager *modemanager = ( IVModeManager * )&g_ModeManager;



// --------------------------------------------------------------------------------- //
// CASWModeManager implementation.
// --------------------------------------------------------------------------------- //

#define SCREEN_FILE		"scripts/vgui_screens.txt"

void CHorizonModeManager::Init()
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		g_pClientMode[ i ] = GetClientModeNormal();
	}

	PanelMetaClassMgr()->LoadMetaClassDefinitionFile( SCREEN_FILE );

	//GetClientVoiceMgr()->SetHeadLabelOffset( 40 );
}

void CHorizonModeManager::LevelInit( const char *newmap )
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetClientMode()->LevelInit( newmap );
	}
}

void CHorizonModeManager::LevelShutdown( void )
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetClientMode()->LevelShutdown();
	}
}

ClientModeHorizon g_ClientModeNormal[ MAX_SPLITSCREEN_PLAYERS ];
IClientMode *GetClientModeNormal()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return &g_ClientModeNormal[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

ClientModeHorizon* GetClientModeHorizon()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return &g_ClientModeNormal[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

// these vgui panels will be closed at various times (e.g. when the level ends/starts)
static char const *s_CloseWindowNames[]={
	"InfoMessageWindow",
	"SkipIntro",
};

//-----------------------------------------------------------------------------
// Purpose: this is the viewport that contains all the hud elements
//-----------------------------------------------------------------------------
class CHudViewport : public CBaseViewport
{
private:
	DECLARE_CLASS_SIMPLE( CHudViewport, CBaseViewport );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		GetHud().InitColors( pScheme );

		SetPaintBackgroundEnabled( false );
	}

	virtual void CreateDefaultPanels( void ) { /* don't create any panels yet*/ };
};
//--------------------------------------------------------------------------------------------------------

// See interface.h/.cpp for specifics:  basically this ensures that we actually Sys_UnloadModule the dll and that we don't call Sys_LoadModule 
//  over and over again.
static CDllDemandLoader g_GameUI( "gameui" );

class FullscreenHorizonViewport : public CHudViewport
{
private:
	DECLARE_CLASS_SIMPLE( FullscreenHorizonViewport, CHudViewport );

private:
	virtual void InitViewportSingletons( void )
	{
		SetAsFullscreenViewportInterface();
	}
};

class ClientModeHorizonFullscreen : public	ClientModeHorizon
{
	DECLARE_CLASS_SIMPLE( ClientModeHorizonFullscreen, ClientModeHorizon );
public:
	virtual void InitViewport()
	{
		// Skip over BaseClass!!!
		BaseClass::BaseClass::InitViewport();
		m_pViewport = new FullscreenHorizonViewport();
		m_pViewport->Start( gameuifuncs, gameeventmanager );
	}
	virtual void Init()
	{
		CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
		if ( gameUIFactory )
		{
			IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
			if ( NULL != pGameUI )
			{
				// insert stats summary panel as the loading background dialog
				CHorizon_Loading_Panel *pPanel = GHorizonLoadingPanel();
				pPanel->InvalidateLayout( false, true );
				pPanel->SetVisible( false );
				pPanel->MakePopup( false );
				pGameUI->SetLoadingBackgroundDialog( pPanel->GetVPanel() );

				// add ASI logo to main menu
				CHorizon_Logo_Panel *pLogo = new CHorizon_Logo_Panel( NULL, "ASILogo" );
				vgui::VPANEL GameUIRoot = enginevgui->GetPanel( PANEL_GAMEUIDLL );
				pLogo->SetParent( GameUIRoot );
				g_hLogoPanel = pLogo;
			}		
		}

		// 
		//CASW_VGUI_Debug_Panel *pDebugPanel = new CASW_VGUI_Debug_Panel( GetViewport(), "ASW Debug Panel" );
		//g_hDebugPanel = pDebugPanel;

		// Skip over BaseClass!!!
		BaseClass::BaseClass::Init();

		// Load up the combine control panel scheme
		if ( !g_hVGuiCombineScheme )
		{
			g_hVGuiCombineScheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), IsXbox() ? "resource/ClientScheme.res" : "resource/CombinePanelScheme.res", "CombineScheme" );
			if (!g_hVGuiCombineScheme)
			{
				Warning( "Couldn't load combine panel scheme!\n" );
			}
		}
	}
	void Shutdown()
	{
		DestroyHorizonLoadingPanel();
		if (g_hLogoPanel.Get())
		{
			delete g_hLogoPanel.Get();
		}
	}
};

//--------------------------------------------------------------------------------------------------------
static ClientModeHorizonFullscreen g_FullscreenClientMode;
IClientMode *GetFullscreenClientMode( void )
{
	return &g_FullscreenClientMode;
}


void ClientModeHorizon::Init()
{
	BaseClass::Init();

	gameeventmanager->AddListener( this, "game_newmap", false );
}
void ClientModeHorizon::Shutdown()
{
	if ( HorizonBackgroundMovie() )
	{
		HorizonBackgroundMovie()->ClearCurrentMovie();
	}
	DestroyHorizonLoadingPanel();
	if (g_hLogoPanel.Get())
	{
		delete g_hLogoPanel.Get();
	}
}

void ClientModeHorizon::InitViewport()
{
	m_pViewport = new CHudViewport();
	m_pViewport->Start( gameuifuncs, gameeventmanager );
}

void ClientModeHorizon::LevelInit( const char *newmap )
{
	// reset ambient light
	static ConVarRef mat_ambient_light_r( "mat_ambient_light_r" );
	static ConVarRef mat_ambient_light_g( "mat_ambient_light_g" );
	static ConVarRef mat_ambient_light_b( "mat_ambient_light_b" );

	if ( mat_ambient_light_r.IsValid() )
	{
		mat_ambient_light_r.SetValue( "0" );
	}

	if ( mat_ambient_light_g.IsValid() )
	{
		mat_ambient_light_g.SetValue( "0" );
	}

	if ( mat_ambient_light_b.IsValid() )
	{
		mat_ambient_light_b.SetValue( "0" );
	}

	BaseClass::LevelInit(newmap);

	// sdk: make sure no windows are left open from before
	Horizon_CloseAllWindows();

	// clear any DSP effects
	CLocalPlayerFilter filter;
	enginesound->SetRoomType( filter, 0 );
	enginesound->SetPlayerDSP( filter, 0, true );
}

void ClientModeHorizon::LevelShutdown( void )
{
	BaseClass::LevelShutdown();

	// sdk:shutdown all vgui windows
	Horizon_CloseAllWindows();
}
void ClientModeHorizon::FireGameEvent( IGameEvent *event )
{
	const char *eventname = event->GetName();

	if ( Q_strcmp( "asw_mission_restart", eventname ) == 0 )
	{
		Horizon_CloseAllWindows();
	}
	else if ( Q_strcmp( "game_newmap", eventname ) == 0 )
	{
		engine->ClientCmd("exec newmapsettings\n");
	}
	else
	{
		BaseClass::FireGameEvent(event);
	}
}
// Close all ASW specific VGUI windows that the player might have open
void ClientModeHorizon::Horizon_CloseAllWindows()
{
	Horizon_CloseAllWindowsFrom(GetViewport());
}

// recursive search for matching window names
void ClientModeHorizon::Horizon_CloseAllWindowsFrom(vgui::Panel* pPanel)
{
	if (!pPanel)
		return;

	int num_names = NELEMS(s_CloseWindowNames);

	for (int k=0;k<pPanel->GetChildCount();k++)
	{
		Panel *pChild = pPanel->GetChild(k);
		if (pChild)
		{
			Horizon_CloseAllWindowsFrom(pChild);
		}
	}

	// When VGUI is shutting down (i.e. if the player closes the window), GetName() can return NULL
	const char *pPanelName = pPanel->GetName();
	if ( pPanelName != NULL )
	{
		for (int i=0;i<num_names;i++)
		{
			if ( !strcmp( pPanelName, s_CloseWindowNames[i] ) )
			{
				pPanel->SetVisible(false);
				pPanel->MarkForDeletion();
			}
		}
	}
}

void ClientModeHorizon::DoPostScreenSpaceEffects( const CViewSetup *pSetup )
{

}