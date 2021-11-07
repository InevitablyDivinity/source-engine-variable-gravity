#ifndef _INCLUDED_HORIZON_INPUT_H
#define _INCLUDED_HORIZON_INPUT_H
#ifdef _WIN32
#pragma once
#endif

#include "input.h"

//-----------------------------------------------------------------------------
// Purpose: ASW Input interface
//-----------------------------------------------------------------------------
class CHorizonInput : public CInput
{
public:
	CHorizonInput();
};

extern CHorizonInput *HorizonInput();

#endif // _INCLUDED_HORIZON_INPUT_H