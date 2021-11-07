#ifndef C_WEAPON_PAINTGUN_H
#define C_WEAPON_PAINTGUN_H

#include "weapon_horizonbase.h"

class C_WeaponPaintGun : public C_WeaponHorizonBase
{
	DECLARE_CLASS(C_WeaponPaintGun, C_WeaponHorizonBase);
	DECLARE_CLIENTCLASS();
public:

	virtual void UpdateTransmitState();

	int m_PaintType;
};

#endif // C_WEAPON_PAINTGUN_H