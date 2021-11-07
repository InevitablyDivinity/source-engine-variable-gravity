#include "cbase.h"
#include "horizon_input.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: ASW Input interface
//-----------------------------------------------------------------------------
static CHorizonInput g_Input;

// Expose this interface
IInput *input = ( IInput * )&g_Input;

CHorizonInput *HorizonInput()
{ 
	return &g_Input;
}

CHorizonInput::CHorizonInput()
{

}