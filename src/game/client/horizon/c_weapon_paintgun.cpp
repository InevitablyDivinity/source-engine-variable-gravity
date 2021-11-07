#include "cbase.h"
#include "weapon_horizonbase.h"
#include "vgui/portal2_hud_paintgun.h"
#include "in_buttons.h"

class C_WeaponPaintGun : public C_WeaponHorizonBase
{
	DECLARE_CLASS(C_WeaponPaintGun, C_WeaponHorizonBase);
	DECLARE_CLIENTCLASS();
public:

	virtual int KeyInput(int down, ButtonCode_t keynum, const char* pszCurrentBinding);

	virtual const char* GetClassname() { return "weapon_paintgun"; }

	virtual void PreDataUpdate(DataUpdateType_t updateType);

	int m_PaintType;
};

IMPLEMENT_CLIENTCLASS_DT(C_WeaponPaintGun, DT_WeaponPaintGun, CWeaponPaintGun)
	RecvPropInt(RECVINFO(m_PaintType)),
END_RECV_TABLE()

LINK_ENTITY_TO_CLASS(weapon_paintgun, C_WeaponPaintGun);

int C_WeaponPaintGun::KeyInput(int down, ButtonCode_t keynum, const char* pszCurrentBinding)
{
	switch (keynum)
	{
	case MOUSE_WHEEL_UP:
		GetHud().m_iKeyBits |= IN_WEAPON1;
		return 0;

	case MOUSE_WHEEL_DOWN:
		GetHud().m_iKeyBits |= IN_WEAPON2;
		return 0;
	}

	// Allow engine to process
	return BaseClass::KeyInput(down, keynum, pszCurrentBinding);
}

void C_WeaponPaintGun::PreDataUpdate(DataUpdateType_t updateType)
{
	if (GetOwner() == C_BasePlayer::GetLocalPlayer())
	{
		if (updateType == DATA_UPDATE_DATATABLE_CHANGED)
		{
			CHudPaintGun* pHUD = GET_HUDELEMENT(CHudPaintGun);
			pHUD->SetPaintType(m_PaintType);
		}
	}
}