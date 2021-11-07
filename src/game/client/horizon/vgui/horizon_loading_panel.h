//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef HORIZON_LOADING_PANEL_H
#define HORIZON_LOADING_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "GameEventListener.h"

class LocationDetailsPanel;
namespace vgui
{
	class Label;
};

class LoadingMissionDetailsPanel: public vgui::EditablePanel
{
private:
	DECLARE_CLASS_SIMPLE( LoadingMissionDetailsPanel, vgui::EditablePanel );

public:
	LoadingMissionDetailsPanel( vgui::Panel *parent, const char *name );
	virtual ~LoadingMissionDetailsPanel();

	virtual void ApplySchemeSettings(vgui::IScheme *scheme);
	void SetGenerationOptions( KeyValues *pKeys );

	vgui::Label *m_pLocationNameLabel;
	vgui::Label *m_pLocationDescriptionLabel;

	KeyValues *m_pGenerationOptions;
};

class CHorizon_Loading_Panel : public vgui::EditablePanel, public CGameEventListener
{
private:
	DECLARE_CLASS_SIMPLE( CHorizon_Loading_Panel, vgui::EditablePanel );

public:
	CHorizon_Loading_Panel();	 
	CHorizon_Loading_Panel( vgui::Panel *parent );
	virtual ~CHorizon_Loading_Panel();	 

	void Init( void );

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();
	virtual void FireGameEvent( IGameEvent *event );

	void SetLoadingMapName( const char *szMapName );
	void SetGenerationOptions( KeyValues *pKeys );

private:
	void GetLoadingScreenSize(int &w, int &t, int &xoffset);

	vgui::ImagePanel* m_pBackdrop;
	vgui::Panel* m_pBlackBar[2];
	char m_szLoadingPic[256];
	LoadingMissionDetailsPanel *m_pDetailsPanel;
};


CHorizon_Loading_Panel *GHorizonLoadingPanel();
void DestroyHorizonLoadingPanel();

#endif // HORIZON_LOADING_PANEL_H
