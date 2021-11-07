#include "cbase.h"
#include "gravity_shared.h"

void GetGravityAxes(Vector vecGravityDir, Vector& gravityAxis, Vector& gravityAxisInverse)
{
	VectorAbs(vecGravityDir, gravityAxis);
	gravityAxisInverse = Vector(1, 1, 1) - gravityAxis;
}