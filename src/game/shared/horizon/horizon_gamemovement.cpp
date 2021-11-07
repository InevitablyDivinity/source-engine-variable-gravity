#include "cbase.h"
#include "in_buttons.h"
#include <stdarg.h>
#include "movevars_shared.h"
#include "engine/IEngineTrace.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "decals.h"
#include "coordsize.h"
#include "rumble_shared.h"
#include "horizon_gamemovement.h"
#ifndef CLIENT_DLL
#include "env_player_surface_trigger.h"
#endif
#include "horizon_player_shared.h"
#include "horizon_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	MAX_CLIP_PLANES		5
#define	NUM_CROUCH_HINTS	3

// Expose our interface.
static CHorizonGameMovement g_GameMovement;
IGameMovement* g_pGameMovement = (IGameMovement*)&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CHorizonGameMovement, IGameMovement, INTERFACENAME_GAMEMOVEMENT, g_GameMovement);

CHorizonGameMovement::CHorizonGameMovement() : CGameMovement()
{
}

// get a conservative bounds for this player's movement traces
// This allows gamemovement to optimize those traces
void CHorizonGameMovement::SetupMovementBounds(CMoveData* move)
{
	if (m_pTraceListData)
	{
		m_pTraceListData->Reset();
	}
	else
	{
		m_pTraceListData = enginetrace->AllocTraceListData();
	}
	if (!move->m_nPlayerHandle.IsValid())
	{
		return;
	}

	CBasePlayer* pPlayer = (CBasePlayer*)move->m_nPlayerHandle.Get();
	
	Vector moveMins, moveMaxs;
	ClearBounds(moveMins, moveMaxs);
	Vector start = move->GetAbsOrigin();
	float radius = ((move->m_vecVelocity.Length() + move->m_flMaxSpeed) * gpGlobals->frametime) + 1.0f;
	// NOTE: assumes the unducked bbox encloses the ducked bbox
	Vector boxMins = GetPlayerMins(false);
	Vector boxMaxs = GetPlayerMaxs(false);

	// bloat by traveling the max velocity in all directions, plus the stepsize up/down
	Vector bloat;
	bloat.Init(radius, radius, radius);
	bloat += pPlayer->m_Local.m_flStepSize * GetGravityDirection();
	AddPointToBounds(start + boxMaxs + bloat, moveMins, moveMaxs);
	AddPointToBounds(start + boxMins - bloat, moveMins, moveMaxs);
	// now build an optimized trace within these bounds
	enginetrace->SetupLeafAndEntityListBox(moveMins, moveMaxs, m_pTraceListData);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHorizonGameMovement::StartGravity(void)
{
	float ent_gravity;

	if (player->GetGravity())
		ent_gravity = player->GetGravity();
	else
		ent_gravity = 1.0;

	// Add gravity so they'll be in the correct position during movement
	// yes, this 0.5 looks wrong, but it's not.  
	mv->m_vecVelocity += GetGravityDirection() * ent_gravity * sv_gravity.GetFloat() * 0.5 * gpGlobals->frametime;
	mv->m_vecVelocity += GetGravityDirection() * player->GetBaseVelocity() * gpGlobals->frametime;

	Vector temp = player->GetBaseVelocity();
	temp *= GetGravityAxisInverse();
	player->SetBaseVelocity(temp);

	CheckVelocity();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHorizonGameMovement::CheckWaterJump(void)
{
	Vector	flatforward;
	Vector	flatvelocity;
	float curspeed;

	Vector forward = m_vecForward;

	// Already water jumping.
	if (player->m_flWaterJumpTime)
		return;

	// Don't hop out if we just jumped in
	if (DeterminedVectorLengthUsingAxis(mv->m_vecVelocity) < -180)
		return; // only hop out if we are moving up

	// See if we are backing up
	flatvelocity = mv->m_vecVelocity;
	flatvelocity *= GetGravityAxisInverse();


	// Must be moving
	curspeed = VectorNormalize(flatvelocity);

	// see if near an edge
	flatforward = forward;
	flatvelocity *= GetGravityAxisInverse();
	VectorNormalize(flatforward);

	// Are we backing into water from steps or something?  If so, don't pop forward
	if (curspeed != 0.0 && (DotProduct(flatvelocity, flatforward) < 0.0))
		return;

	Vector vecStart;
	// Start line trace at waist height (using the center of the player for this here)
	vecStart = mv->GetAbsOrigin() + (GetPlayerMins() + GetPlayerMaxs()) * 0.5;

	Vector vecEnd;
	VectorMA(vecStart, 24.0f, flatforward, vecEnd);

	trace_t tr;
	TracePlayerBBox(vecStart, vecEnd, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, tr);
	if (tr.fraction < 1.0)		// solid at waist
	{
		IPhysicsObject* pPhysObj = tr.m_pEnt->VPhysicsGetObject();
		if (pPhysObj)
		{
			if (pPhysObj->GetGameFlags() & FVPHYSICS_PLAYER_HELD)
				return;
		}

		vecStart = GetGravityAxis() * (mv->GetAbsOrigin() + player->GetViewOffset()) + WATERJUMP_HEIGHT * GetGravityAxis();
		VectorMA(vecStart, 24.0f, flatforward, vecEnd);
		VectorMA(vec3_origin, -50.0f, tr.plane.normal, player->m_vecWaterJumpVel);

		TracePlayerBBox(vecStart, vecEnd, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, tr);
		if (tr.fraction == 1.0)		// open at eye level
		{
			// Now trace down to see if we would actually land on a standable surface.
			VectorCopy(vecEnd, vecStart);
			mv->m_vecVelocity += GetGravityDirection() * 1024.0f;
			TracePlayerBBox(vecStart, vecEnd, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, tr);
			if ((tr.fraction < 1.0f) && (DotProduct(tr.plane.normal, -GetGravityDirection()) >= 0.7))
			{
				mv->m_vecVelocity *= GetGravityAxisInverse();
				mv->m_vecVelocity += GetGravityDirection() * 256;
				mv->m_nOldButtons |= IN_JUMP;		// Don't jump again until released
				player->AddFlag(FL_WATERJUMP);
				player->m_flWaterJumpTime = 2000.0f;	// Do this for 2 seconds
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHorizonGameMovement::WaterJump(void)
{
	if (player->m_flWaterJumpTime > 10000)
		player->m_flWaterJumpTime = 10000;

	if (!player->m_flWaterJumpTime)
		return;

	player->m_flWaterJumpTime -= 1000.0f * gpGlobals->frametime;

	if (player->m_flWaterJumpTime <= 0 || !player->GetWaterLevel())
	{
		player->m_flWaterJumpTime = 0;
		player->RemoveFlag(FL_WATERJUMP);
	}

	mv->m_vecVelocity = player->m_vecWaterJumpVel;
	mv->m_vecVelocity *= GetGravityAxisInverse();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHorizonGameMovement::WaterMove(void)
{
	int		i;
	Vector	wishvel;
	float	wishspeed;
	Vector	wishdir;
	Vector	start, dest;
	Vector  temp;
	trace_t	pm;
	float speed, newspeed, addspeed, accelspeed;
	Vector forward, right, up;

	/*AngleVectors(mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles
	matrix3x4_t matGravityTransform = GetGravityTransform();
	VectorRotate(forward, matGravityTransform, forward);
	VectorRotate(right, matGravityTransform, right);
	VectorRotate(up, matGravityTransform, up);*/
	player->EyeVectors(&forward, &right, &up);

	//
	// user intentions
	//
	for (i = 0; i < 3; i++)
	{
		wishvel[i] = forward[i] * mv->m_flForwardMove + right[i] * mv->m_flSideMove;
	}

	// if we have the jump key down, move us up as well
	if (mv->m_nButtons & IN_JUMP)
	{
		wishvel += mv->m_flClientMaxSpeed * GetGravityDirection();
	}
	// Sinking after no other movement occurs
	else if (!mv->m_flForwardMove && !mv->m_flSideMove && !mv->m_flUpMove)
	{
		wishvel += 60 * GetGravityDirection();		// drift towards bottom
	}
	else  // Go straight up by upmove amount.
	{
		// exaggerate upward movement along forward as well
		float upwardMovememnt = DeterminedVectorLengthUsingAxis(mv->m_flForwardMove * forward * 2);
		upwardMovememnt = clamp(upwardMovememnt, 0, mv->m_flClientMaxSpeed);
		wishvel += GetGravityDirection() * (mv->m_flUpMove + upwardMovememnt);
	}

	// Copy it over and determine speed
	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	// Cap speed.
	if (wishspeed > mv->m_flMaxSpeed)
	{
		VectorScale(wishvel, mv->m_flMaxSpeed / wishspeed, wishvel);
		wishspeed = mv->m_flMaxSpeed;
	}

	// Slow us down a bit.
	wishspeed *= 0.8;

	// Water friction
	VectorCopy(mv->m_vecVelocity, temp);
	speed = VectorNormalize(temp);
	if (speed)
	{
		newspeed = speed - gpGlobals->frametime * speed * sv_friction.GetFloat() * player->m_surfaceFriction;
		if (newspeed < 0.1f)
		{
			newspeed = 0;
		}

		VectorScale(mv->m_vecVelocity, newspeed / speed, mv->m_vecVelocity);
	}
	else
	{
		newspeed = 0;
	}

	// water acceleration
	if (wishspeed >= 0.1f)  // old !
	{
		addspeed = wishspeed - newspeed;
		if (addspeed > 0)
		{
			VectorNormalize(wishvel);
			accelspeed = sv_accelerate.GetFloat() * wishspeed * gpGlobals->frametime * player->m_surfaceFriction;
			if (accelspeed > addspeed)
			{
				accelspeed = addspeed;
			}

			for (i = 0; i < 3; i++)
			{
				float deltaSpeed = accelspeed * wishvel[i];
				mv->m_vecVelocity[i] += deltaSpeed;
				mv->m_outWishVel[i] += deltaSpeed;
			}
		}
	}

	VectorAdd(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);

	// Now move
	// assume it is a stair or a slope, so press down from stepheight above
	VectorMA(mv->GetAbsOrigin(), gpGlobals->frametime, mv->m_vecVelocity, dest);

	TracePlayerBBox(mv->GetAbsOrigin(), dest, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm);
	if (pm.fraction == 1.0f)
	{
		VectorCopy(dest, start);
		if (player->m_Local.m_bAllowAutoMovement)
		{
			start += GetGravityDirection() * (player->m_Local.m_flStepSize + 1);
		}

		TracePlayerBBox(start, dest, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm);

		if (!pm.startsolid && !pm.allsolid)
		{
			float stepDist = DeterminedVectorLengthUsingAxis(pm.endpos - mv->GetAbsOrigin());
			mv->m_outStepHeight += stepDist;
			// walked up the step, so just keep result and exit
			mv->SetAbsOrigin(pm.endpos);
			VectorSubtract(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);
			return;
		}

		// Try moving straight along out normal path.
		TryPlayerMove();
	}
	else
	{
		if (!player->GetGroundEntity())
		{
			TryPlayerMove();
			VectorSubtract(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);
			return;
		}

		StepMove(dest, pm);
	}

	VectorSubtract(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);
}

//-----------------------------------------------------------------------------
// Purpose: Does the basic move attempting to climb up step heights.  It uses
//          the mv->GetAbsOrigin() and mv->m_vecVelocity.  It returns a new
//          new mv->GetAbsOrigin(), mv->m_vecVelocity, and mv->m_outStepHeight.
//-----------------------------------------------------------------------------
void CHorizonGameMovement::StepMove(Vector& vecDestination, trace_t& trace)
{
	//
	// Save the move position and velocity in case we need to put it back later.
	//
	Vector vecPos, vecVel;
	VectorCopy(mv->GetAbsOrigin(), vecPos);
	VectorCopy(mv->m_vecVelocity, vecVel);

	//
	// First try walking straight to where they want to go.
	//
	Vector vecEndPos;
	VectorCopy(vecDestination, vecEndPos);
	TryPlayerMove(&vecEndPos, &trace);

	//
	// mv now contains where they ended up if they tried to walk straight there.
	// Save those results for use later.
	//	
	Vector vecDownPos, vecDownVel;
	VectorCopy(mv->GetAbsOrigin(), vecDownPos);
	VectorCopy(mv->m_vecVelocity, vecDownVel);

	//
	// Reset original values to try some other things.
	//
	mv->SetAbsOrigin(vecPos);
	VectorCopy(vecVel, mv->m_vecVelocity);

	//
	// Move up a stair height.
	// Slide forward at the same velocity but from the higher position.
	//
	VectorCopy(mv->GetAbsOrigin(), vecEndPos);
	if (player->m_Local.m_bAllowAutoMovement)
	{
		vecEndPos += GetGravityDirection() * (player->m_Local.m_flStepSize + DIST_EPSILON);
	}

	// Only step up as high as we have headroom to do so.	
	TracePlayerBBox(mv->GetAbsOrigin(), vecEndPos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace);
	if (!trace.startsolid && !trace.allsolid)
	{
		mv->SetAbsOrigin(trace.endpos);
	}
	TryPlayerMove();

	//
	// Move down a stair (attempt to).
	// Slide forward at the same velocity from the lower position.
	//
	VectorCopy(mv->GetAbsOrigin(), vecEndPos);
	if (player->m_Local.m_bAllowAutoMovement)
	{
		vecEndPos += GetGravityDirection() * (player->m_Local.m_flStepSize + DIST_EPSILON);
	}

	TracePlayerBBox(mv->GetAbsOrigin(), vecEndPos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace);

	// If we are not on the ground any more then use the original movement attempt.
	if (DotProduct(trace.plane.normal, -GetGravityDirection()) > 0.7)
	{
		mv->SetAbsOrigin(vecDownPos);
		VectorCopy(vecDownVel, mv->m_vecVelocity);
		float flStepDist = DeterminedVectorLengthUsingAxis(mv->GetAbsOrigin() - vecPos);
		if (flStepDist > 0.0f)
		{
			mv->m_outStepHeight += flStepDist;
		}
		return;
	}

	// If the trace ended up in empty space, copy the end over to the origin.
	if (!trace.startsolid && !trace.allsolid)
	{
		mv->SetAbsOrigin(trace.endpos);
	}

	// Copy this origin to up.
	Vector vecUpPos;
	VectorCopy(mv->GetAbsOrigin(), vecUpPos);

	// decide which one went farther
	// TODO: what's this
//	float flDownDist = (vecDownPos.x - vecPos.x) * (vecDownPos.x - vecPos.x) + (vecDownPos.y - vecPos.y) * (vecDownPos.y - vecPos.y);
//	float flUpDist = (vecUpPos.x - vecPos.x) * (vecUpPos.x - vecPos.x) + (vecUpPos.y - vecPos.y) * (vecUpPos.y - vecPos.y);
	Vector vecPosNoGravAxis = vecPos * GetGravityAxisInverse();
	float flDownDist = VectorLength(vecDownPos * GetGravityAxisInverse() - vecPosNoGravAxis);
	float flUpDist = VectorLength(vecUpPos * vecUpPos *= GetGravityAxisInverse() - vecPosNoGravAxis);
	if (flDownDist > flUpDist)
	{
		mv->SetAbsOrigin(vecDownPos);
		VectorCopy(vecDownVel, mv->m_vecVelocity);
	}
	else
	{
		// copy z value from slide move
		mv->m_vecVelocity *= GetGravityAxisInverse();
		mv->m_vecVelocity += vecDownVel * GetGravityDirection();
	}

	float flStepDist = DeterminedVectorLengthUsingAxis(mv->GetAbsOrigin() - vecPos);
	if (flStepDist > 0)
	{
		mv->m_outStepHeight += flStepDist;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHorizonGameMovement::FinishGravity(void)
{
	float ent_gravity;

	if (player->m_flWaterJumpTime)
		return;

	if (player->GetGravity())
		ent_gravity = player->GetGravity();
	else
		ent_gravity = 1.0;

	// Get the correct velocity for the end of the dt 
	mv->m_vecVelocity += GetGravityDirection() * ent_gravity * sv_gravity.GetFloat() * gpGlobals->frametime * 0.5;

	CheckVelocity();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHorizonGameMovement::AirMove(void)
{
	int			i;
	Vector		wishvel;
	float		fmove, smove;
	Vector		wishdir;
	float		wishspeed;
	Vector forward, right, up;

	/*AngleVectors(mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles
	matrix3x4_t matGravityTransform = GetGravityTransform();
	VectorRotate(forward, matGravityTransform, forward);
	VectorRotate(right, matGravityTransform, right);
	VectorRotate(up, matGravityTransform, up);*/
	player->EyeVectors(&forward, &right, &up);

	// Copy movement amounts
	fmove = mv->m_flForwardMove;
	smove = mv->m_flSideMove;

	// Zero out z components of movement vectors
	forward *= GetGravityAxisInverse();
	right *= GetGravityAxisInverse();
	VectorNormalize(forward);  // Normalize remainder of vectors
	VectorNormalize(right);    // 

	for (i = 0; i < 3; i++)       // Determine x and y parts of velocity
		wishvel[i] = forward[i] * fmove + right[i] * smove;
	wishvel *= GetGravityAxisInverse(); // Zero out z part of velocity

	VectorCopy(wishvel, wishdir);   // Determine maginitude of speed of move
	wishspeed = VectorNormalize(wishdir);

	//
	// clamp to server defined max speed
	//
	if (wishspeed != 0 && (wishspeed > mv->m_flMaxSpeed))
	{
		VectorScale(wishvel, mv->m_flMaxSpeed / wishspeed, wishvel);
		wishspeed = mv->m_flMaxSpeed;
	}

	AirAccelerate(wishdir, wishspeed, sv_airaccelerate.GetFloat());

	// Add in any base velocity to the current velocity.
	VectorAdd(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);

	TryPlayerMove();

	// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
	VectorSubtract(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);
}


//-----------------------------------------------------------------------------
// Purpose: Try to keep a walking player on the ground when running down slopes etc
//-----------------------------------------------------------------------------
void CHorizonGameMovement::StayOnGround(void)
{
	trace_t trace;
	Vector start(mv->GetAbsOrigin());
	Vector end(mv->GetAbsOrigin());
	start -= GetGravityDirection() * 2;
	end += GetGravityDirection() * player->GetStepSize();

	// See how far up we can go without getting stuck

	TracePlayerBBox(mv->GetAbsOrigin(), start, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace);
	start = trace.endpos;

	// using trace.startsolid is unreliable here, it doesn't get set when
	// tracing bounding box vs. terrain

	// Now trace down from a known safe position
	TracePlayerBBox(start, end, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace);
	if (trace.fraction > 0.0f &&			// must go somewhere
		trace.fraction < 1.0f &&			// must hit something
		!trace.startsolid &&				// can't be embedded in a solid
		DotProduct(trace.plane.normal, -GetGravityDirection()) >= 0.7)		// can't hit a steep slope that we can't stand on anyway
	{
		float flDelta = DeterminedVectorLengthUsingAxis(mv->GetAbsOrigin() - trace.endpos);

		//This is incredibly hacky. The real problem is that trace returning that strange value we can't network over.
		if (flDelta > 0.5f * COORD_RESOLUTION)
		{
			mv->SetAbsOrigin(trace.endpos);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHorizonGameMovement::WalkMove(void)
{
	int i;

	Vector wishvel;
	float spd;
	float fmove, smove;
	Vector wishdir;
	float wishspeed;

	Vector dest;
	trace_t pm;
	Vector forward, right, up;

	/*AngleVectors(mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles
	matrix3x4_t matGravityTransform = GetGravityTransform();
	VectorRotate(forward, matGravityTransform, forward);
	VectorRotate(right, matGravityTransform, right);
	VectorRotate(up, matGravityTransform, up);*/
	player->EyeVectors(&forward, &right, &up);

	CHandle< CBaseEntity > oldground;
	oldground = player->GetGroundEntity();

	// Copy movement amounts
	fmove = mv->m_flForwardMove;
	smove = mv->m_flSideMove;

	// Zero out z components of movement vectors
	forward *= GetGravityAxisInverse();
	right *= GetGravityAxisInverse();

	VectorNormalize(forward);  // Normalize remainder of vectors.
	VectorNormalize(right);    // 

	for (i = 0; i < 3; i++)       // Determine x and y parts of velocity
		wishvel[i] = forward[i] * fmove + right[i] * smove;

	wishvel *= GetGravityAxisInverse();            // Zero out z part of velocity

	VectorCopy(wishvel, wishdir);   // Determine maginitude of speed of move
	wishspeed = VectorNormalize(wishdir);

	//
	// Clamp to server defined max speed
	//
	if ((wishspeed != 0.0f) && (wishspeed > mv->m_flMaxSpeed))
	{
		VectorScale(wishvel, mv->m_flMaxSpeed / wishspeed, wishvel);
		wishspeed = mv->m_flMaxSpeed;
	}

	// Set pmove velocity
	mv->m_vecVelocity *= GetGravityAxisInverse();
	Accelerate(wishdir, wishspeed, sv_accelerate.GetFloat());
	mv->m_vecVelocity *= GetGravityAxisInverse(); // BOOKMARK

	// Add in any base velocity to the current velocity.
	VectorAdd(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);

	spd = VectorLength(mv->m_vecVelocity);

	if (spd < 1.0f)
	{
		mv->m_vecVelocity.Init();
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);
		return;
	}

	// first try just moving to the destination	
	/*
		dest[0] = mv->GetAbsOrigin()[0] + mv->m_vecVelocity[0]*gpGlobals->frametime;
		dest[1] = mv->GetAbsOrigin()[1] + mv->m_vecVelocity[1]*gpGlobals->frametime;	
		dest[2] = mv->GetAbsOrigin()[2];
	*/
	dest += GetGravityAxisInverse() * (mv->GetAbsOrigin() + mv->m_vecVelocity) * gpGlobals->frametime;
	dest += mv->GetAbsOrigin() * GetGravityDirection();

	// first try moving directly to the next spot
	TracePlayerBBox(mv->GetAbsOrigin(), dest, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm);

	// If we made it all the way, then copy trace end as new player position.
	mv->m_outWishVel += wishdir * wishspeed;

	if (pm.fraction == 1)
	{
		mv->SetAbsOrigin(pm.endpos);
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);

		StayOnGround();
		return;
	}

	// Don't walk up stairs if not on ground.
	if (oldground == NULL && player->GetWaterLevel() == 0)
	{
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);
		return;
	}

	// If we are jumping out of water, don't do anything more.
	if (player->m_flWaterJumpTime)
	{
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);
		return;
	}

	StepMove(dest, pm);

	// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
	VectorSubtract(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);

	StayOnGround();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHorizonGameMovement::FullWalkMove()
{
	if (!CheckWater())
	{
		StartGravity();
	}

	// If we are leaping out of the water, just update the counters.
	if (player->m_flWaterJumpTime)
	{
		WaterJump();
		TryPlayerMove();
		// See if we are still in water?
		CheckWater();
		return;
	}

	// If we are swimming in the water, see if we are nudging against a place we can jump up out
	//  of, and, if so, start out jump.  Otherwise, if we are not moving up, then reset jump timer to 0
	if (player->GetWaterLevel() >= WL_Waist)
	{
		if (player->GetWaterLevel() == WL_Waist)
		{
			CheckWaterJump();
		}

		// If we are falling again, then we must not trying to jump out of water any more.
		if (DeterminedVectorLengthUsingAxis(mv->m_vecVelocity) < 0 &&
			player->m_flWaterJumpTime)
		{
			player->m_flWaterJumpTime = 0;
		}

		// Was jump button pressed?
		if (mv->m_nButtons & IN_JUMP)
		{
			CheckJumpButton();
		}
		else
		{
			mv->m_nOldButtons &= ~IN_JUMP;
		}

		// Perform regular water movement
		WaterMove();

		// Redetermine position vars
		CategorizePosition();

		// If we are on ground, no downward velocity.
		if (player->GetGroundEntity() != NULL)
		{
			mv->m_vecVelocity *= GetGravityAxisInverse();
		}
	}
	else
		// Not fully underwater
	{
		// Was jump button pressed?
		if (mv->m_nButtons & IN_JUMP)
		{
			CheckJumpButton();
		}
		else
		{
			mv->m_nOldButtons &= ~IN_JUMP;
		}

		// Fricion is handled before we add in any base velocity. That way, if we are on a conveyor, 
		//  we don't slow when standing still, relative to the conveyor.
		if (player->GetGroundEntity() != NULL)
		{
			mv->m_vecVelocity *= GetGravityAxisInverse();
			player->m_Local.m_flFallVelocity = 0.0f;
			Friction();
		}

		// Make sure velocity is valid.
		CheckVelocity();

		if (player->GetGroundEntity() != NULL)
		{
			WalkMove();
		}
		else
		{
			AirMove();  // Take into account movement when in air.
		}

		// Set final flags.
		CategorizePosition();

		// Make sure velocity is valid.
		CheckVelocity();

		// Add any remaining gravitational component.
		if (!CheckWater())
		{
			FinishGravity();
		}

		// If we are on ground, no downward velocity.
		if (player->GetGroundEntity() != NULL)
		{
			mv->m_vecVelocity *= GetGravityAxisInverse();
		}
		CheckFalling();
	}

	if ((m_nOldWaterLevel == WL_NotInWater && player->GetWaterLevel() != WL_NotInWater) ||
		(m_nOldWaterLevel != WL_NotInWater && player->GetWaterLevel() == WL_NotInWater))
	{
		PlaySwimSound();
#if !defined( CLIENT_DLL )
		player->Splash();
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHorizonGameMovement::CheckJumpButton(void)
{
	if (player->pl.deadflag)
	{
		mv->m_nOldButtons |= IN_JUMP;	// don't jump again until released
		return false;
	}

	// See if we are waterjumping.  If so, decrement count and return.
	if (player->m_flWaterJumpTime)
	{
		player->m_flWaterJumpTime -= gpGlobals->frametime;
		if (player->m_flWaterJumpTime < 0)
			player->m_flWaterJumpTime = 0;

		return false;
	}

	// If we are in the water most of the way...
	if (player->GetWaterLevel() >= 2)
	{
		// swimming, not jumping
		SetGroundEntity(NULL);

		if (player->GetWaterType() == CONTENTS_WATER)    // We move up a certain amount
		{
			mv->m_vecVelocity *= GetGravityAxisInverse();
			mv->m_vecVelocity += -GetGravityDirection() * 100;
		}
		else if (player->GetWaterType() == CONTENTS_SLIME)
		{
			mv->m_vecVelocity *= GetGravityAxisInverse();
			mv->m_vecVelocity += -GetGravityDirection() * 80;
		}

		// play swiming sound
		if (player->m_flSwimSoundTime <= 0)
		{
			// Don't play sound again for 1 second
			player->m_flSwimSoundTime = 1000;
			PlaySwimSound();
		}

		return false;
	}

	// No more effect
	if (player->GetGroundEntity() == NULL)
	{
		mv->m_nOldButtons |= IN_JUMP;
		return false;		// in air, so no effect
	}

	// Don't allow jumping when the player is in a stasis field.
	if (player->m_Local.m_bSlowMovement)
		return false;

	if (mv->m_nOldButtons & IN_JUMP)
		return false;		// don't pogo stick

	// Cannot jump will in the unduck transition.
	if (player->m_Local.m_bDucking && (player->GetFlags() & FL_DUCKING))
		return false;

	// Still updating the eye position.
	if (player->m_Local.m_nDuckJumpTimeMsecs > 0)
		return false;


	// In the air now.
	SetGroundEntity(NULL);

	player->PlayStepSound((Vector&)mv->GetAbsOrigin(), player->m_pSurfaceData, 1.0, true);

	MoveHelper()->PlayerSetAnimation(PLAYER_JUMP);

	float flGroundFactor = 1.0f;
	if (player->m_pSurfaceData)
	{
		flGroundFactor = player->m_pSurfaceData->game.jumpFactor;
	}

	float flMul = sqrt(2 * sv_gravity.GetFloat() * GAMEMOVEMENT_JUMP_HEIGHT);

	// Acclerate upward
	// If we are ducking...
	Vector startg = mv->m_vecVelocity * GetGravityDirection();
	if ((player->m_Local.m_bDucking) || (player->GetFlags() & FL_DUCKING))
	{
		// d = 0.5 * g * t^2		- distance traveled with linear accel
		// t = sqrt(2.0 * 45 / g)	- how long to fall 45 units
		// v = g * t				- velocity at the end (just invert it to jump up that high)
		// v = g * sqrt(2.0 * 45 / g )
		// v^2 = g * g * 2.0 * 45 / g
		// v = sqrt( g * 2.0 * 45 )
		mv->m_vecVelocity *= GetGravityAxisInverse();
		mv->m_vecVelocity += -GetGravityDirection() * flGroundFactor * flMul; // 2 * gravity * height
	}
	else
	{
		mv->m_vecVelocity += -GetGravityDirection() * flGroundFactor * flMul;  // 2 * gravity * height
	}

	FinishGravity();

	mv->m_outJumpVel += (mv->m_vecVelocity * GetGravityAxis()) - startg;
	mv->m_outStepHeight += 0.15f;


	bool bSetDuckJump = (gpGlobals->maxClients == 1); //most games we only set duck jump if the game is single player


	// Set jump time.
	if (bSetDuckJump)
	{
		player->m_Local.m_nJumpTimeMsecs = GAMEMOVEMENT_JUMP_TIME;
		player->m_Local.m_bInDuckJump = true;
	}

	// Flag that we jumped.
	mv->m_nOldButtons |= IN_JUMP;	// don't jump again until released
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CHorizonGameMovement::TryPlayerMove(Vector* pFirstDest, trace_t* pFirstTrace)
{
	int			bumpcount, numbumps;
	Vector		dir;
	float		d;
	int			numplanes;
	Vector		planes[MAX_CLIP_PLANES];
	Vector		primal_velocity, original_velocity;
	Vector      new_velocity;
	int			i, j;
	trace_t	pm;
	Vector		end;
	float		time_left, allFraction;
	int			blocked;

	numbumps = 4;           // Bump up to four times

	blocked = 0;           // Assume not blocked
	numplanes = 0;           //  and not sliding along any planes

	VectorCopy(mv->m_vecVelocity, original_velocity);  // Store original velocity
	VectorCopy(mv->m_vecVelocity, primal_velocity);

	allFraction = 0;
	time_left = gpGlobals->frametime;   // Total time for this movement operation.

	new_velocity.Init();

	for (bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		if (mv->m_vecVelocity.Length() == 0.0)
			break;

		// Assume we can move all the way from the current origin to the
		//  end point.
		VectorMA(mv->GetAbsOrigin(), time_left, mv->m_vecVelocity, end);

		// See if we can make it from origin to end point.
		TracePlayerBBox(mv->GetAbsOrigin(), end, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm);

		allFraction += pm.fraction;

		// If we started in a solid object, or we were in solid space
		//  the whole way, zero out our velocity and return that we
		//  are blocked by floor and wall.
		if (pm.allsolid)
		{
			// entity is trapped in another solid
			VectorCopy(vec3_origin, mv->m_vecVelocity);
			return 4;
		}

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove.origin and 
		//  zero the plane counter.
		if (pm.fraction > 0)
		{
			if (numbumps > 0 && pm.fraction == 1)
			{
				// There's a precision issue with terrain tracing that can cause a swept box to successfully trace
				// when the end position is stuck in the triangle.  Re-run the test with an uswept box to catch that
				// case until the bug is fixed.
				// If we detect getting stuck, don't allow the movement
				trace_t stuck;
				TracePlayerBBox(pm.endpos, pm.endpos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, stuck);
				if (stuck.startsolid || stuck.fraction != 1.0f)
				{
					//Msg( "Player will become stuck!!!\n" );
					VectorCopy(vec3_origin, mv->m_vecVelocity);
					break;
				}
			}

			// actually covered some distance
			mv->SetAbsOrigin(pm.endpos);
			VectorCopy(mv->m_vecVelocity, original_velocity);
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		if (pm.fraction == 1)
		{
			break;		// moved the entire distance
		}

		// Save entity that blocked us (since fraction was < 1.0)
		//  for contact
		// Add it if it's not already in the list!!!
		MoveHelper()->AddToTouched(pm, mv->m_vecVelocity);

		// If the plane we hit has a high z component in the normal, then
		//  it's probably a floor
		if (DotProduct(pm.plane.normal, -GetGravityDirection()) > 0.7)
		{
			blocked |= 1;		// floor
		}
		// If the plane has a zero z component in the normal, then it's a 
		//  step or wall
		if (DotProduct(pm.plane.normal, -GetGravityDirection()) != 0)
		{
			blocked |= 2;		// step / wall
		}

		// Reduce amount of m_flFrameTime left by total time left * fraction
		//  that we covered.
		time_left -= time_left * pm.fraction;

		// Did we run out of planes to clip against?
		if (numplanes >= MAX_CLIP_PLANES)
		{
			// this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy(vec3_origin, mv->m_vecVelocity);
			//Con_DPrintf("Too many planes 4\n");

			break;
		}

		// Set up next clipping plane
		VectorCopy(pm.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		//

		// reflect player velocity 
		// Only give this a try for first impact plane because you can get yourself stuck in an acute corner by jumping in place
		//  and pressing forward and nobody was really using this bounce/reflection feature anyway...
		if (numplanes == 1 &&
			player->GetMoveType() == MOVETYPE_WALK &&
			player->GetGroundEntity() == NULL)
		{
			for (i = 0; i < numplanes; i++)
			{
				if (DotProduct(planes[i], -GetGravityDirection()) > 0.7)
				{
					// floor or slope
					ClipVelocity(original_velocity, planes[i], new_velocity, 1);
					VectorCopy(new_velocity, original_velocity);
				}
				else
				{
					ClipVelocity(original_velocity, planes[i], new_velocity, 1.0 + sv_bounce.GetFloat() * (1 - player->m_surfaceFriction));
				}
			}

			VectorCopy(new_velocity, mv->m_vecVelocity);
			VectorCopy(new_velocity, original_velocity);
		}
		else
		{
			for (i = 0; i < numplanes; i++)
			{
				ClipVelocity(original_velocity, planes[i], mv->m_vecVelocity, 1);

				for (j = 0; j < numplanes; j++)
				{
					if (j != i)
					{
						// Are we now moving against this plane?
						if (mv->m_vecVelocity.Dot(planes[j]) < 0)
							break;	// not ok
					}
				}

				if (j == numplanes)  // Didn't have to clip, so we're ok
					break;
			}

			// Did we go all the way through plane set
			if (i != numplanes)
			{	// go along this plane
				// pmove.velocity is set in clipping call, no need to set again.
				;
			}
			else
			{	// go along the crease
				if (numplanes != 2)
				{
					VectorCopy(vec3_origin, mv->m_vecVelocity);
					break;
				}
				CrossProduct(planes[0], planes[1], dir);
				dir.NormalizeInPlace();
				d = dir.Dot(mv->m_vecVelocity);
				VectorScale(dir, d, mv->m_vecVelocity);
			}

			//
			// if original velocity is against the original velocity, stop dead
			// to avoid tiny occilations in sloping corners
			//
			d = mv->m_vecVelocity.Dot(primal_velocity);
			if (d <= 0)
			{
				//Con_DPrintf("Back\n");
				VectorCopy(vec3_origin, mv->m_vecVelocity);
				break;
			}
		}
	}

	if (allFraction == 0)
	{
		VectorCopy(vec3_origin, mv->m_vecVelocity);
	}

	// Check if they slammed into a wall
	float fSlamVol = 0.0f;

	float fLateralStoppingAmount = primal_velocity.Length2D() - mv->m_vecVelocity.Length2D();
	if (fLateralStoppingAmount > PLAYER_MAX_SAFE_FALL_SPEED * 2.0f)
	{
		fSlamVol = 1.0f;
	}
	else if (fLateralStoppingAmount > PLAYER_MAX_SAFE_FALL_SPEED)
	{
		fSlamVol = 0.85f;
	}

	if (fSlamVol > 0.0f)
	{
		PlayerRoughLandingEffects(fSlamVol);
	}

	return blocked;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHorizonGameMovement::LadderMove(void)
{
	trace_t pm;
	bool onFloor;
	Vector floor;
	Vector wishdir;
	Vector end;

	if (player->GetMoveType() == MOVETYPE_NOCLIP)
		return false;

	if (!GameHasLadders())
		return false;

	// If I'm already moving on a ladder, use the previous ladder direction
	if (player->GetMoveType() == MOVETYPE_LADDER)
	{
		wishdir = -player->m_vecLadderNormal.Get();
	}
	else
	{
		// otherwise, use the direction player is attempting to move
		if (mv->m_flForwardMove || mv->m_flSideMove)
		{
			for (int i = 0; i < 3; i++)       // Determine x and y parts of velocity
				wishdir[i] = m_vecForward[i] * mv->m_flForwardMove + m_vecRight[i] * mv->m_flSideMove;

			VectorNormalize(wishdir);
		}
		else
		{
			// Player is not attempting to move, no ladder behavior
			return false;
		}
	}

	// wishdir points toward the ladder if any exists
	VectorMA(mv->GetAbsOrigin(), LadderDistance(), wishdir, end);
	TracePlayerBBox(mv->GetAbsOrigin(), end, LadderMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm);

	// no ladder in that direction, return
	if (pm.fraction == 1.0f || !OnLadder(pm))
		return false;

	player->SetMoveType(MOVETYPE_LADDER);
	player->SetMoveCollide(MOVECOLLIDE_DEFAULT);

	player->m_vecLadderNormal = pm.plane.normal;

	// On ladder, convert movement to be relative to the ladder

	VectorCopy(mv->GetAbsOrigin(), floor);
	floor += GetPlayerMins() * GetGravityDirection() - 1 * GetGravityDirection();

	if (enginetrace->GetPointContents(floor) == CONTENTS_SOLID || player->GetGroundEntity() != NULL)
	{
		onFloor = true;
	}
	else
	{
		onFloor = false;
	}

	player->SetGravity(0);

	float climbSpeed = ClimbSpeed();

	float forwardSpeed = 0, rightSpeed = 0;
	if (mv->m_nButtons & IN_BACK)
		forwardSpeed -= climbSpeed;

	if (mv->m_nButtons & IN_FORWARD)
		forwardSpeed += climbSpeed;

	if (mv->m_nButtons & IN_MOVELEFT)
		rightSpeed -= climbSpeed;

	if (mv->m_nButtons & IN_MOVERIGHT)
		rightSpeed += climbSpeed;

	if (mv->m_nButtons & IN_JUMP)
	{
		player->SetMoveType(MOVETYPE_WALK);
		player->SetMoveCollide(MOVECOLLIDE_DEFAULT);

		VectorScale(pm.plane.normal, 270, mv->m_vecVelocity);
	}
	else
	{
		if (forwardSpeed != 0 || rightSpeed != 0)
		{
			Vector velocity, perp, cross, lateral, tmp;

			//ALERT(at_console, "pev %.2f %.2f %.2f - ",
			//	pev->velocity.x, pev->velocity.y, pev->velocity.z);
			// Calculate player's intended velocity
			//Vector velocity = (forward * gpGlobals->v_forward) + (right * gpGlobals->v_right);
			VectorScale(m_vecForward, forwardSpeed, velocity);
			VectorMA(velocity, rightSpeed, m_vecRight, velocity);

			// Perpendicular in the ladder plane
			VectorCopy(vec3_origin, tmp);
			tmp *= GetGravityAxisInverse();
			tmp += 1 * GetGravityDirection();
			CrossProduct(tmp, pm.plane.normal, perp);
			VectorNormalize(perp);

			// decompose velocity into ladder plane
			float normal = DotProduct(velocity, pm.plane.normal);

			// This is the velocity into the face of the ladder
			VectorScale(pm.plane.normal, normal, cross);

			// This is the player's additional velocity
			VectorSubtract(velocity, cross, lateral);

			// This turns the velocity into the face of the ladder into velocity that
			// is roughly vertically perpendicular to the face of the ladder.
			// NOTE: It IS possible to face up and move down or face down and move up
			// because the velocity is a sum of the directional velocity and the converted
			// velocity through the face of the ladder -- by design.
			CrossProduct(pm.plane.normal, perp, tmp);
			VectorMA(lateral, -normal, tmp, mv->m_vecVelocity);

			if (onFloor && normal > 0)	// On ground moving away from the ladder
			{
				VectorMA(mv->m_vecVelocity, MAX_CLIMB_SPEED, pm.plane.normal, mv->m_vecVelocity);
			}
			//pev->velocity = lateral - (CrossProduct( trace.vecPlaneNormal, perp ) * normal);
		}
		else
		{
			mv->m_vecVelocity.Init();
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHorizonGameMovement::AddGravity(void)
{
	if (player->m_flWaterJumpTime)
		return;

	float ent_gravity;

	if (player->m_flWaterJumpTime)
		return;

	if (player->GetGravity())
		ent_gravity = player->GetGravity();
	else
		ent_gravity = 1.0;

	// Add gravity incorrectly
	mv->m_vecVelocity -= (GetGravityDirection() * ent_gravity * sv_gravity.GetFloat() * gpGlobals->frametime);
	mv->m_vecVelocity += (GetGravityDirection() * player->GetBaseVelocity() * gpGlobals->frametime);

	Vector temp = player->GetBaseVelocity();
	temp *= GetGravityAxisInverse();
	player->SetBaseVelocity(temp);

	CheckVelocity();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : in - 
//			normal - 
//			out - 
//			overbounce - 
// Output : int
//-----------------------------------------------------------------------------
int CHorizonGameMovement::ClipVelocity(Vector& in, Vector& normal, Vector& out, float overbounce)
{
	float	backoff;
	float	change;
	float angle;
	int		i, blocked;

	angle = DotProduct(-GetGravityDirection(), normal); //normal[2]; // TODO

	blocked = 0x00;         // Assume unblocked.
	if (angle > 0)			// If the plane that is blocking us has a positive z component, then assume it's a floor.
		blocked |= 0x01;	// 
	if (!angle)				// If the plane has no Z, it is vertical (wall/step)
		blocked |= 0x02;	// 


	// Determine how far along plane to slide based on incoming direction.
	backoff = DotProduct(in, normal) * overbounce;

	for (i = 0; i < 3; i++)
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;
	}

	// iterate once to make sure we aren't still moving through the plane
	float adjust = DotProduct(out, normal);
	if (adjust < 0.0f)
	{
		out -= (normal * adjust);
		//		Msg( "Adjustment = %lf\n", adjust );
	}

	// Return blocking flags.
	return blocked;
}

//-----------------------------------------------------------------------------
// Purpose: returns the height to check for water
//-----------------------------------------------------------------------------
void CHorizonGameMovement::GetWaterCheckPosition(int waterLevel, Vector* pos)
{
	(*pos) = GetGravityAxisInverse() * (mv->GetAbsOrigin() + (GetPlayerMins() + GetPlayerMaxs())) * 0.5;

	switch (waterLevel)
	{
	case WL_Eyes:
		(*pos) = GetGravityDirection() * (mv->GetAbsOrigin() + player->GetViewOffset());
		return;

	case WL_Waist:
		(*pos) = GetGravityDirection() * (mv->GetAbsOrigin() + (GetPlayerMins() + GetPlayerMaxs())) * 0.5;
		return;

	default:
		(*pos) = GetGravityDirection() * (mv->GetAbsOrigin() + GetPlayerMins()) + GetGravityDirection() * 1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &input - 
// Output : bool
//-----------------------------------------------------------------------------
bool CHorizonGameMovement::CheckWater(void)
{
	Vector	point;
	int		cont;

	// Pick a spot just above the players feet.
	GetWaterCheckPosition(WL_Feet, &point);

	// Assume that we are not in water at all.
	player->SetWaterLevel(WL_NotInWater);
	player->SetWaterType(CONTENTS_EMPTY);

	// Grab point contents.
	cont = GetWaterContentsForPointCached(point, 0);

	// Are we under water? (not solid and not empty?)
	if (cont & MASK_WATER)
	{
		// Set water type
		player->SetWaterType(cont);

		// We are at least at level one
		player->SetWaterLevel(WL_Feet);

		// Now check a point that is at the player hull midpoint.
		GetWaterCheckPosition(WL_Waist, &point);
		cont = GetWaterContentsForPointCached(point, 1);
		// If that point is also under water...
		if (cont & MASK_WATER)
		{
			// Set a higher water level.
			player->SetWaterLevel(WL_Waist);

			// Now check the eye position.  (view_ofs is relative to the origin)
			GetWaterCheckPosition(WL_Eyes, &point);
			cont = GetWaterContentsForPointCached(point, 2);
			if (cont & MASK_WATER)
				player->SetWaterLevel(WL_Eyes);  // In over our eyes
		}

		// Adjust velocity based on water current, if any.
		if (cont & MASK_CURRENT)
		{
			Vector v;
			VectorClear(v);
			if (cont & CONTENTS_CURRENT_0)
				v[0] += 1;
			if (cont & CONTENTS_CURRENT_90)
				v[1] += 1;
			if (cont & CONTENTS_CURRENT_180)
				v[0] -= 1;
			if (cont & CONTENTS_CURRENT_270)
				v[1] -= 1;
			if (cont & CONTENTS_CURRENT_UP)
				v[2] += 1;
			if (cont & CONTENTS_CURRENT_DOWN)
				v[2] -= 1;

			// BUGBUG -- this depends on the value of an unspecified enumerated type
			// The deeper we are, the stronger the current.
			Vector temp;
			VectorMA(player->GetBaseVelocity(), 50.0 * player->GetWaterLevel(), v, temp);
			player->SetBaseVelocity(temp);
		}
	}

	// if we just transitioned from not in water to in water, record the time it happened
	if ((WL_NotInWater == m_nOldWaterLevel) && (player->GetWaterLevel() > WL_NotInWater))
	{
		m_flWaterEntryTime = gpGlobals->curtime;
	}

	return (player->GetWaterLevel() > WL_Feet);
}

void CHorizonGameMovement::SetGroundEntity(trace_t* pm)
{
	CBaseEntity* newGround = pm ? pm->m_pEnt : NULL;

	CBaseEntity* oldGround = player->GetGroundEntity();
	Vector vecBaseVelocity = player->GetBaseVelocity();

	if (!oldGround && newGround)
	{
		// Subtract ground velocity at instant we hit ground jumping
		vecBaseVelocity -= newGround->GetAbsVelocity();

		vecBaseVelocity *= GetGravityAxisInverse();
		vecBaseVelocity += newGround->GetAbsVelocity() * GetGravityDirection();
	}
	else if (oldGround && !newGround)
	{
		// Add in ground velocity at instant we started jumping
		vecBaseVelocity += oldGround->GetAbsVelocity();

		vecBaseVelocity *= GetGravityAxisInverse();
		vecBaseVelocity += oldGround->GetAbsVelocity() * GetGravityDirection();
	}

	player->SetBaseVelocity(vecBaseVelocity);
	player->SetGroundEntity(newGround);

	// If we are on something...

	if (newGround)
	{
		CategorizeGroundSurface(*pm);

		// Then we are not in water jump sequence
		player->m_flWaterJumpTime = 0;

		// Standing on an entity other than the world, so signal that we are touching something.
		if (!pm->DidHitWorld())
		{
			MoveHelper()->AddToTouched(*pm, mv->m_vecVelocity);
		}

		if (player->GetMoveType() != MOVETYPE_NOCLIP)
			mv->m_vecVelocity *= GetGravityAxisInverse();
	}
}

static inline void DoTrace(ITraceListData* pTraceListData, const Ray_t& ray, uint32 fMask, ITraceFilter* filter, trace_t* ptr, int* counter)
{
	++* counter;

	if (pTraceListData && pTraceListData->CanTraceRay(ray))
	{
		enginetrace->TraceRayAgainstLeafAndEntityList(ray, pTraceListData, fMask, filter, ptr);
	}
	else
	{
		enginetrace->TraceRay(ray, fMask, filter, ptr);
	}
}

//-----------------------------------------------------------------------------
// Traces the player's collision bounds in quadrants, looking for a plane that
// can be stood upon (normal's z >= flStandableZ).  Regardless of success or failure,
// replace the fraction and endpos with the original ones, so we don't try to
// move the player down to the new floor and get stuck on a leaning wall that
// the original trace hit first.
//-----------------------------------------------------------------------------
void CHorizonGameMovement::TracePlayerBBoxForGround(ITraceListData* pTraceListData, const Vector& start,
	const Vector& end, const Vector& minsSrc,
	const Vector& maxsSrc, unsigned int fMask,
	ITraceFilter* filter, trace_t& pm, float minGroundNormalZ, bool overwriteEndpos, int* pCounter)
{
	VPROF("TracePlayerBBoxForGround");
	// minGroundNormalZ
	Ray_t ray;
	Vector mins, maxs;

	float fraction = pm.fraction;
	Vector endpos = pm.endpos;

	// Check the -x, -y quadrant
	mins = minsSrc;
	maxs.Init(MIN(0, maxsSrc.x), MIN(0, maxsSrc.y), maxsSrc.z); // TODO: reorient for the player
	ray.Init(start, end, mins, maxs);
	DoTrace(pTraceListData, ray, fMask, filter, &pm, pCounter);
	if (pm.m_pEnt && DotProduct(pm.plane.normal, -GetGravityDirection()) >= minGroundNormalZ)
	{
		if (overwriteEndpos)
		{
			pm.fraction = fraction;
			pm.endpos = endpos;
		}
		return;
	}

	// Check the +x, +y quadrant
	mins.Init(MAX(0, minsSrc.x), MAX(0, minsSrc.y), minsSrc.z); // TODO: reorient for the player
	maxs = maxsSrc;
	ray.Init(start, end, mins, maxs);
	DoTrace(pTraceListData, ray, fMask, filter, &pm, pCounter);
	if (pm.m_pEnt && DotProduct(pm.plane.normal, -GetGravityDirection()) >= minGroundNormalZ)
	{
		if (overwriteEndpos)
		{
			pm.fraction = fraction;
			pm.endpos = endpos;
		}
		return;
	}

	// Check the -x, +y quadrant
	mins.Init(minsSrc.x, MAX(0, minsSrc.y), minsSrc.z); // TODO: reorient for the player
	maxs.Init(MIN(0, maxsSrc.x), maxsSrc.y, maxsSrc.z); // TODO: reorient for the player
	ray.Init(start, end, mins, maxs);
	DoTrace(pTraceListData, ray, fMask, filter, &pm, pCounter);
	if (pm.m_pEnt && DotProduct(pm.plane.normal, -GetGravityDirection()) >= 0.7)
	{
		if (overwriteEndpos)
		{
			pm.fraction = fraction;
			pm.endpos = endpos;
		}
		return;
	}

	// Check the +x, -y quadrant
	mins.Init(MAX(0, minsSrc.x), minsSrc.y, minsSrc.z); // TODO: Account for player orientation
	maxs.Init(maxsSrc.x, MIN(0, maxsSrc.y), maxsSrc.z);
	ray.Init(start, end, mins, maxs);
	DoTrace(pTraceListData, ray, fMask, filter, &pm, pCounter);
	if (pm.m_pEnt && DotProduct(pm.plane.normal, -GetGravityDirection()) >= minGroundNormalZ)
	{
		if (overwriteEndpos)
		{
			pm.fraction = fraction;
			pm.endpos = endpos;
		}
		return;
	}

	if (overwriteEndpos)
	{
		pm.fraction = fraction;
		pm.endpos = endpos;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHorizonGameMovement::CategorizePosition(void)
{
	Vector point;
	trace_t pm;

	// Reset this each time we-recategorize, otherwise we have bogus friction when we jump into water and plunge downward really quickly
	player->m_surfaceFriction = 1.0f;

	// if the player hull point one unit down is solid, the player
	// is on ground

	// see if standing on something solid	

	// Doing this before we move may introduce a potential latency in water detection, but
	// doing it after can get us stuck on the bottom in water if the amount we move up
	// is less than the 1 pixel 'threshold' we're about to snap to.	Also, we'll call
	// this several times per frame, so we really need to avoid sticking to the bottom of
	// water on each call, and the converse case will correct itself if called twice.
	CheckWater();

	// observers don't have a ground entity
	if (player->IsObserver())
		return;

	float flOffset = 2.0f;

	point = mv->GetAbsOrigin();
//	point *= GetGravityAxisInverse();
	point += GetGravityDirection() * flOffset;

	Vector bumpOrigin;
	bumpOrigin = mv->GetAbsOrigin();

	// Shooting up really fast.  Definitely not on ground.
	// On ladder moving up, so not on ground either
	// NOTE: 145 is a jump.
#define NON_JUMP_VELOCITY 140.0f

	float gvel = DeterminedVectorLengthUsingAxis(mv->m_vecVelocity);
	bool bMovingUp = gvel > 0;
	bool bMovingUpRapidly = gvel > NON_JUMP_VELOCITY;
	float flGroundEntityVel;
	if (bMovingUpRapidly)
	{
		// Tracker 73219, 75878:  ywb 8/2/07
		// After save/restore (and maybe at other times), we can get a case where we were saved on a lift and 
		//  after restore we'll have a high local velocity due to the lift making our abs velocity appear high.  
		// We need to account for standing on a moving ground object in that case in order to determine if we really 
		//  are moving away from the object we are standing on at too rapid a speed.  Note that CheckJump already sets
		//  ground entity to NULL, so this wouldn't have any effect unless we are moving up rapidly not from the jump button.
		CBaseEntity* ground = player->GetGroundEntity();
		if (ground)
		{
			flGroundEntityVel = DeterminedVectorLengthUsingAxis(ground->GetAbsVelocity());
			bMovingUpRapidly = (gvel - flGroundEntityVel) > NON_JUMP_VELOCITY;
		}
	}

	// NOTE YWB 7/5/07:  Since we're already doing a traceline here, we'll subsume the StayOnGround (stair debouncing) check into the main traceline we do here to see what we're standing on
	bool bUnderwater = (player->GetWaterLevel() >= WL_Eyes);
	bool bMoveToEndPos = false;
	if (player->GetMoveType() == MOVETYPE_WALK &&
		player->GetGroundEntity() != NULL && !bUnderwater)
	{
		// if walking and still think we're on ground, we'll extend trace down by stepsize so we don't bounce down slopes
		bMoveToEndPos = true;

		point += GetGravityDirection() * player->m_Local.m_flStepSize;
	}

	// Was on ground, but now suddenly am not
	if (bMovingUpRapidly ||
		(bMovingUp && player->GetMoveType() == MOVETYPE_LADDER))
	{
		SetGroundEntity(NULL);
		bMoveToEndPos = false;
	}
	else
	{
		// Try and move down.
		TracePlayerBBox(bumpOrigin, point, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm);

		// Was on ground, but now suddenly am not.  If we hit a steep plane, we are not on ground
		float flStandableZ = 0.7;

		if (!pm.m_pEnt || DotProduct(pm.plane.normal, -GetGravityDirection()) < flStandableZ)
		{
			// Test four sub-boxes, to see if any of them would have found shallower slope we could actually stand on
			ITraceFilter* pFilter = LockTraceFilter(COLLISION_GROUP_PLAYER_MOVEMENT);
			TracePlayerBBoxForGround(m_pTraceListData, bumpOrigin, point, GetPlayerMins(),
				GetPlayerMaxs(), PlayerSolidMask(), pFilter, pm, flStandableZ, true, &m_nTraceCount);
			UnlockTraceFilter(pFilter);
			if (!pm.m_pEnt || DotProduct(pm.plane.normal, -GetGravityDirection()) < flStandableZ)
			{
				SetGroundEntity(NULL);
				// probably want to add a check for a +z velocity too!
				if (DeterminedVectorLengthUsingAxis(mv->m_vecVelocity) > 0.0f &&
					(player->GetMoveType() != MOVETYPE_NOCLIP))
				{
					player->m_surfaceFriction = 0.25f;
				}
				bMoveToEndPos = false;
			}
			else
			{
				SetGroundEntity(&pm);
			}
		}
		else
		{
			SetGroundEntity(&pm);  // Otherwise, point to index of ent under us.
		}

#ifndef CLIENT_DLL

		//Adrian: vehicle code handles for us.
		if (player->IsInAVehicle() == false)
		{
			// If our gamematerial has changed, tell any player surface triggers that are watching
			IPhysicsSurfaceProps* physprops = MoveHelper()->GetSurfaceProps();
			surfacedata_t* pSurfaceProp = physprops->GetSurfaceData(pm.surface.surfaceProps);
			char cCurrGameMaterial = pSurfaceProp->game.material;
			if (!player->GetGroundEntity())
			{
				cCurrGameMaterial = 0;
			}

			// Changed?
			if (player->m_chPreviousTextureType != cCurrGameMaterial)
			{
				CEnvPlayerSurfaceTrigger::SetPlayerSurface(player, cCurrGameMaterial);
			}

			player->m_chPreviousTextureType = cCurrGameMaterial;
		}
#endif
	}

	// YWB:  This logic block essentially lifted from StayOnGround implementation
	if (bMoveToEndPos &&
		!pm.startsolid &&				// not sure we need this check as fraction would == 0.0f?
		pm.fraction > 0.0f &&			// must go somewhere
		pm.fraction < 1.0f) 			// must hit something
	{
		mv->SetAbsOrigin(pm.endpos);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determine if the player has hit the ground while falling, apply
//			damage, and play the appropriate impact sound.
//-----------------------------------------------------------------------------
void CHorizonGameMovement::CheckFalling(void)
{
	if (player->GetGroundEntity() != NULL &&
		!IsDead() &&
		player->m_Local.m_flFallVelocity >= PLAYER_FALL_PUNCH_THRESHOLD)
	{
		bool bAlive = true;
		float fvol = 0.5;

		if (player->GetWaterLevel() > 0)
		{
			// They landed in water.
		}
		else
		{
			// Scale it down if we landed on something that's floating...
			if (player->GetGroundEntity()->IsFloating())
			{
				player->m_Local.m_flFallVelocity -= PLAYER_LAND_ON_FLOATING_OBJECT;
			}

			//
			// They hit the ground.
			//
			if (DeterminedVectorLengthUsingAxis(player->GetGroundEntity()->GetAbsVelocity()) < 0.0f)
			{
				// Player landed on a descending object. Subtract the velocity of the ground entity.
				player->m_Local.m_flFallVelocity += DeterminedVectorLengthUsingAxis(player->GetGroundEntity()->GetAbsVelocity());
				player->m_Local.m_flFallVelocity = MAX(0.1f, player->m_Local.m_flFallVelocity);
			}

			if (player->m_Local.m_flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED)
			{
				//
				// If they hit the ground going this fast they may take damage (and die).
				//
				bAlive = MoveHelper()->PlayerFallingDamage();
				fvol = 1.0;
			}
			else if (player->m_Local.m_flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED / 2)
			{
				fvol = 0.85;
			}
			else if (player->m_Local.m_flFallVelocity < PLAYER_MIN_BOUNCE_SPEED)
			{
				fvol = 0;
			}
		}

		PlayerRoughLandingEffects(fvol);

		if (bAlive)
		{
			MoveHelper()->PlayerSetAnimation(PLAYER_WALK);
		}
	}



	//
	// Clear the fall velocity so the impact doesn't happen again.
	//
	if (player->GetGroundEntity() != NULL)
	{
		player->m_Local.m_flFallVelocity = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determine if crouch/uncrouch caused player to get stuck in world
// Input  : direction - 
//-----------------------------------------------------------------------------
void CHorizonGameMovement::FixPlayerCrouchStuck(bool upward)
{
	EntityHandle_t hitent;
	int i;
	Vector test;
	trace_t dummy;

	int direction = upward ? 1 : 0;

	hitent = TestPlayerPosition(mv->GetAbsOrigin(), COLLISION_GROUP_PLAYER_MOVEMENT, dummy);
	if (hitent == INVALID_ENTITY_HANDLE)
		return;

	VectorCopy(mv->GetAbsOrigin(), test);
	for (i = 0; i < 36; i++)
	{
		Vector org = mv->GetAbsOrigin();
		org += -GetGravityDirection() * direction;
		mv->SetAbsOrigin(org);
		hitent = TestPlayerPosition(mv->GetAbsOrigin(), COLLISION_GROUP_PLAYER_MOVEMENT, dummy);
		if (hitent == INVALID_ENTITY_HANDLE)
			return;
	}

	mv->SetAbsOrigin(test); // Failed
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHorizonGameMovement::FinishUnDuckJump(trace_t& trace)
{
	Vector vecNewOrigin;
	VectorCopy(mv->GetAbsOrigin(), vecNewOrigin);

	//  Up for uncrouching.
	Vector hullSizeNormal = GetPlayerMaxs(false) - GetPlayerMins(false);
	Vector hullSizeCrouch = GetPlayerMaxs(true) - GetPlayerMins(true);
	Vector viewDelta = (hullSizeNormal - hullSizeCrouch);

	float flDeltaZ = VectorLength(GetGravityAxis() * viewDelta);
	viewDelta = (viewDelta * GetGravityAxisInverse()) + (viewDelta * GetGravityAxis() * trace.fraction);
//	viewDelta.z *= trace.fraction;
	flDeltaZ -= VectorLength(viewDelta * GetGravityAxis());

	player->RemoveFlag(FL_DUCKING);
	player->m_Local.m_bDucked = false;
	player->m_Local.m_bDucking = false;
	player->m_Local.m_bInDuckJump = false;
	player->m_Local.m_nDuckTimeMsecs = 0;
	player->m_Local.m_nDuckJumpTimeMsecs = 0;
	player->m_Local.m_nJumpTimeMsecs = 0;

	Vector vecViewOffset = GetPlayerViewOffset(false);
	vecViewOffset += flDeltaZ * GetGravityDirection(); // lower view offset
	player->SetViewOffset(vecViewOffset);

	VectorSubtract(vecNewOrigin, viewDelta, vecNewOrigin);
	mv->SetAbsOrigin(vecNewOrigin);

	// Recategorize position since ducking can change origin
	CategorizePosition();
}

//
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : duckFraction - 
//-----------------------------------------------------------------------------
void CHorizonGameMovement::SetDuckedEyeOffset(float duckFraction)
{
	Vector vDuckHullMin = GetPlayerMins(true);
	Vector vStandHullMin = GetPlayerMins(false);

	float fMore = VectorLength(GetGravityAxis() * (vDuckHullMin - vStandHullMin)); // BROKENNNNNNNNNNNNNNNN

	float flDuckViewOffset = HorizonGameRules()->GetDuckingViewOffset();
	float flStandViewOffset = HorizonGameRules()->GetStandingViewOffset();

	Vector temp = player->GetViewOffset();
	temp *= GetGravityAxisInverse();
	float flDuckFraction = ((flDuckViewOffset - fMore) * duckFraction + flStandViewOffset * (1 - duckFraction));
	temp += -GetGravityDirection() * flDuckFraction;
	
	player->SetViewOffset(temp);
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if we are in a situation where we can unduck jump.
//-----------------------------------------------------------------------------
bool CHorizonGameMovement::CanUnDuckJump(trace_t& trace)
{
	// Trace down to the stand position and see if we can stand.
	Vector vecEnd(mv->GetAbsOrigin());
	vecEnd -= GetGravityDirection() * 36.0f;		// This will have to change if bounding hull change!
	TracePlayerBBox(mv->GetAbsOrigin(), vecEnd, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace);
	if (trace.fraction < 1.0f)
	{
		// Find the endpoint.
		vecEnd *= GetGravityAxisInverse();
		vecEnd += mv->GetAbsOrigin() + (GetGravityDirection() * -36.0f * trace.fraction);

		// Test a normal hull.
		trace_t traceUp;
		bool bWasDucked = player->m_Local.m_bDucked;
		player->m_Local.m_bDucked = false;
		TracePlayerBBox(vecEnd, vecEnd, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, traceUp);
		player->m_Local.m_bDucked = bWasDucked;
		if (!traceUp.startsolid)
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: See if duck button is pressed and do the appropriate things
//-----------------------------------------------------------------------------
void CHorizonGameMovement::Duck(void)
{
	int buttonsChanged = (mv->m_nOldButtons ^ mv->m_nButtons);	// These buttons have changed this frame
	int buttonsPressed = buttonsChanged & mv->m_nButtons;			// The changed ones still down are "pressed"
	int buttonsReleased = buttonsChanged & mv->m_nOldButtons;		// The changed ones which were previously down are "released"

	// Check to see if we are in the air.
	bool bInAir = (player->GetGroundEntity() == NULL);
	bool bInDuck = (player->GetFlags() & FL_DUCKING) ? true : false;
	bool bDuckJump = (player->m_Local.m_nJumpTimeMsecs > 0);
	bool bDuckJumpTime = (player->m_Local.m_nDuckJumpTimeMsecs > 0);

	if (mv->m_nButtons & IN_DUCK)
	{
		mv->m_nOldButtons |= IN_DUCK;
	}
	else
	{
		mv->m_nOldButtons &= ~IN_DUCK;
	}

	// Handle death.
	if (IsDead())
		return;

	// Slow down ducked players.
	HandleDuckingSpeedCrop();

	// If the player is holding down the duck button, the player is in duck transition, ducking, or duck-jumping.
	if ((mv->m_nButtons & IN_DUCK) || player->m_Local.m_bDucking || bInDuck || bDuckJump)
	{
		// DUCK
		if ((mv->m_nButtons & IN_DUCK) || bDuckJump)
		{
			// XBOX SERVER ONLY
#if !defined(CLIENT_DLL)
			if (IsX360() && buttonsPressed & IN_DUCK)
			{
				// Hinting logic
				if (player->GetToggledDuckState() && player->m_nNumCrouches < NUM_CROUCH_HINTS)
				{
					UTIL_HudHintText(player, "#Valve_Hint_Crouch");
					player->m_nNumCrouches++;
				}
			}
#endif
			// Have the duck button pressed, but the player currently isn't in the duck position.
			if ((buttonsPressed & IN_DUCK) && !bInDuck && !bDuckJump && !bDuckJumpTime)
			{
				player->m_Local.m_nDuckTimeMsecs = GAMEMOVEMENT_DUCK_TIME;
				player->m_Local.m_bDucking = true;
			}

			// The player is in duck transition and not duck-jumping.
			if (player->m_Local.m_bDucking && !bDuckJump && !bDuckJumpTime)
			{
				int nDuckMilliseconds = MAX(0, GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_nDuckTimeMsecs);

				// Finish in duck transition when transition time is over, in "duck", in air.
				if ((nDuckMilliseconds > TIME_TO_DUCK_MSECS) || bInDuck || bInAir)
				{
					FinishDuck();
				}
				else
				{
					// Calc parametric time
					float flDuckFraction = SimpleSpline(FractionDucked(nDuckMilliseconds));
					SetDuckedEyeOffset(flDuckFraction);
				}
			}

			if (bDuckJump)
			{
				// Make the bounding box small immediately.
				if (!bInDuck)
				{
					StartUnDuckJump();
				}
				else
				{
					// Check for a crouch override.
					if (!(mv->m_nButtons & IN_DUCK))
					{
						trace_t trace;
						if (CanUnDuckJump(trace))
						{

							FinishUnDuckJump(trace);
							player->m_Local.m_nDuckJumpTimeMsecs = (int)(((float)GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS * (1.0f - trace.fraction)) + (float)GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS_INV);
						}
					}
				}
			}
		}
		// UNDUCK (or attempt to...)
		else
		{
			if (player->m_Local.m_bInDuckJump)
			{
				// Check for a crouch override.
				if (!(mv->m_nButtons & IN_DUCK))
				{
					trace_t trace;
					if (CanUnDuckJump(trace))
					{
						FinishUnDuckJump(trace);

						if (trace.fraction < 1.0f)
						{
							player->m_Local.m_nDuckJumpTimeMsecs = (int)(((float)GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS * (1.0f - trace.fraction)) + (float)GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS_INV);
						}
					}
				}
				else
				{
					player->m_Local.m_bInDuckJump = false;
				}
			}

			if (bDuckJumpTime)
				return;

			// Try to unduck unless automovement is not allowed
			// NOTE: When not onground, you can always unduck
			if (player->m_Local.m_bAllowAutoMovement || bInAir || player->m_Local.m_bDucking)
			{
				// We released the duck button, we aren't in "duck" and we are not in the air - start unduck transition.
				if ((buttonsReleased & IN_DUCK))
				{
					if (bInDuck && !bDuckJump)
					{
						player->m_Local.m_nDuckTimeMsecs = GAMEMOVEMENT_DUCK_TIME;
					}
					else if (player->m_Local.m_bDucking && !player->m_Local.m_bDucked)
					{
						// Invert time if release before fully ducked!!!
						int elapsedMilliseconds = GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_nDuckTimeMsecs;

						float fracDucked = FractionDucked(elapsedMilliseconds);
						int remainingUnduckMilliseconds = (int)(fracDucked * TIME_TO_UNDUCK_MSECS);

						player->m_Local.m_nDuckTimeMsecs = GAMEMOVEMENT_DUCK_TIME - TIME_TO_UNDUCK_MSECS + remainingUnduckMilliseconds;
					}
				}


				// Check to see if we are capable of unducking.
				if (CanUnduck())
				{
					// or unducking
					if ((player->m_Local.m_bDucking || player->m_Local.m_bDucked))
					{
						int nDuckMilliseconds = MAX(0, GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_nDuckTimeMsecs);

						// Finish ducking immediately if duck time is over or not on ground
						if (nDuckMilliseconds > TIME_TO_UNDUCK_MSECS || (bInAir && !bDuckJump))
						{
							FinishUnDuck();
						}
						else
						{
							// Calc parametric time
							float flDuckFraction = SimpleSpline(1.0f - FractionUnDucked(nDuckMilliseconds));
							SetDuckedEyeOffset(flDuckFraction);
							player->m_Local.m_bDucking = true;
						}
					}
				}
				else
				{
					// Still under something where we can't unduck, so make sure we reset this timer so
					//  that we'll unduck once we exit the tunnel, etc.
					if (player->m_Local.m_nDuckTimeMsecs != GAMEMOVEMENT_DUCK_TIME)
					{
						SetDuckedEyeOffset(1.0f);
						player->m_Local.m_nDuckTimeMsecs = GAMEMOVEMENT_DUCK_TIME;
						player->m_Local.m_bDucked = true;
						player->m_Local.m_bDucking = false;
						player->AddFlag(FL_DUCKING);
					}
				}
			}
		}
	}
	// HACK: (jimd 5/25/2006) we have a reoccuring bug (#50063 in Tracker) where the player's
	// view height gets left at the ducked height while the player is standing, but we haven't
	// been  able to repro it to find the cause.  It may be fixed now due to a change I'm
	// also making in UpdateDuckJumpEyeOffset but just in case, this code will sense the 
	// problem and restore the eye to the proper position.  It doesn't smooth the transition,
	// but it is preferable to leaving the player's view too low.
	//
	// If the player is still alive and not an observer, check to make sure that
	// his view height is at the standing height.
	else if (!IsDead() && !player->IsObserver() && !player->IsInAVehicle())
	{
		if ((player->m_Local.m_nDuckJumpTimeMsecs == 0) && DeterminedVectorLengthUsingAxis(player->GetViewOffset() - GetPlayerViewOffset(false)) > 0.1)
		{
			// we should rarely ever get here, so assert so a coder knows when it happens
			AssertMsgOnce(0, "Restoring player view height\n");

			// set the eye height to the non-ducked height
			SetDuckedEyeOffset(0.0f);
		}
	}
}

static ConVar sv_optimizedmovement("sv_optimizedmovement", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHorizonGameMovement::PlayerMove(void)
{
	VPROF("CPortal2GameMovement::PlayerMove");

	if (m_bShouldUpdateMovementBounds)
		SetupMovementBounds(mv);

	CheckParameters();

	// clear output applied velocity
	mv->m_outWishVel.Init();
	mv->m_outJumpVel.Init();

	MoveHelper()->ResetTouchList();                    // Assume we don't touch anything

	ReduceTimers();

#ifdef INFESTED_DLL		// ignore roll component for Alien Swarm, this is used for vertical aiming
	QAngle vecViewAngles = mv->m_vecViewAngles;
	vecViewAngles[ROLL] = 0;
	AngleVectors(vecViewAngles, &m_vecForward, &m_vecRight, &m_vecUp);  // Determine movement angles
#else
	/*AngleVectors(mv->m_vecViewAngles, &m_vecForward, &m_vecRight, &m_vecUp);  // Determine movement angles
	matrix3x4_t matGravityTransform = GetGravityTransform();
	VectorRotate(m_vecForward, matGravityTransform, m_vecForward);
	VectorRotate(m_vecRight, matGravityTransform, m_vecRight);
	VectorRotate(m_vecUp, matGravityTransform, m_vecUp);*/
	player->EyeVectors(&m_vecForward, &m_vecRight, &m_vecUp);
#endif

	// Always try and unstick us unless we are using a couple of the movement modes
	MoveType_t moveType = player->GetMoveType();
	if (moveType != MOVETYPE_NOCLIP &&
		moveType != MOVETYPE_NONE &&
		moveType != MOVETYPE_ISOMETRIC &&
		moveType != MOVETYPE_OBSERVER &&
		!player->pl.deadflag)
	{
		if (CheckInterval(STUCK))
		{
			if (CheckStuck())
			{
				// Can't move, we're stuck
				return;
			}
		}
	}

	// Now that we are "unstuck", see where we are (player->GetWaterLevel() and type, player->GetGroundEntity()).
	if (player->GetMoveType() != MOVETYPE_WALK ||
		mv->m_bGameCodeMovedPlayer ||
		!sv_optimizedmovement.GetBool())
	{
		CategorizePosition();
	}
	else
	{
		if (DeterminedVectorLengthUsingAxis(mv->m_vecVelocity) > 250.0f)
		{
			SetGroundEntity(NULL);
		}
	}

	// Store off the starting water level
	m_nOldWaterLevel = player->GetWaterLevel();

	// If we are not on ground, store off how fast we are moving down
	if (player->GetGroundEntity() == NULL)
	{
		player->m_Local.m_flFallVelocity = -DeterminedVectorLengthUsingAxis(mv->m_vecVelocity);
	}

	m_nOnLadder = 0;

	player->UpdateStepSound(player->m_pSurfaceData, mv->GetAbsOrigin(), mv->m_vecVelocity);

	UpdateDuckJumpEyeOffset();
	Duck();

	// Don't run ladder code if dead on on a train
	if (!player->pl.deadflag && !(player->GetFlags() & FL_ONTRAIN))
	{
		// If was not on a ladder now, but was on one before, 
		//  get off of the ladder

		// TODO: this causes lots of weirdness.
		//bool bCheckLadder = CheckInterval( LADDER );
		//if ( bCheckLadder || player->GetMoveType() == MOVETYPE_LADDER )
		{
			if (!LadderMove() &&
				(player->GetMoveType() == MOVETYPE_LADDER))
			{
				// Clear ladder stuff unless player is dead or riding a train
				// It will be reset immediately again next frame if necessary
				player->SetMoveType(MOVETYPE_WALK);
				player->SetMoveCollide(MOVECOLLIDE_DEFAULT);
			}
		}
	}

	// Handle movement modes.
	switch (player->GetMoveType())
	{
	case MOVETYPE_NONE:
		break;

	case MOVETYPE_NOCLIP:
		FullNoClipMove(sv_noclipspeed.GetFloat(), sv_noclipaccelerate.GetFloat());
		break;

	case MOVETYPE_FLY:
	case MOVETYPE_FLYGRAVITY:
		FullTossMove();
		break;

	case MOVETYPE_LADDER:
		FullLadderMove();
		break;

	case MOVETYPE_WALK:
		FullWalkMove();
		break;

	case MOVETYPE_ISOMETRIC:
		//IsometricMove();
		// Could also try:  FullTossMove();
		FullWalkMove();
		break;

	case MOVETYPE_OBSERVER:
		FullObserverMove(); // clips against world&players
		break;

	default:
		DevMsg(1, "Bogus pmove player movetype %i on (%i) 0=cl 1=sv\n", player->GetMoveType(), player->IsServer());
		break;
	}
}


//-----------------------------------------------------------------------------
// Performs the collision resolution for fliers.
//-----------------------------------------------------------------------------
void CHorizonGameMovement::PerformFlyCollisionResolution(trace_t& pm, Vector& move)
{
	Vector base;
	float vel;
	float backoff;

	switch (player->GetMoveCollide())
	{
	case MOVECOLLIDE_FLY_CUSTOM:
		// Do nothing; the velocity should have been modified by touch
		// FIXME: It seems wrong for touch to modify velocity
		// given that it can be called in a number of places
		// where collision resolution do *not* in fact occur

		// Should this ever occur for players!?
		Assert(0);
		break;

	case MOVECOLLIDE_FLY_BOUNCE:
	case MOVECOLLIDE_DEFAULT:
	{
		if (player->GetMoveCollide() == MOVECOLLIDE_FLY_BOUNCE)
			backoff = 2.0 - player->m_surfaceFriction;
		else
			backoff = 1;

		ClipVelocity(mv->m_vecVelocity, pm.plane.normal, mv->m_vecVelocity, backoff);
	}
	break;

	default:
		// Invalid collide type!
		Assert(0);
		break;
	}

	// stop if on ground
	if (DotProduct(pm.plane.normal, -GetGravityDirection()) > 0.7)
	{
		base.Init();
		if (DeterminedVectorLengthUsingAxis(mv->m_vecVelocity) < sv_gravity.GetFloat() * gpGlobals->frametime)
		{
			// we're rolling on the ground, add static friction.
			SetGroundEntity(&pm);
			mv->m_vecVelocity *= GetGravityAxisInverse();
		}

		vel = DotProduct(mv->m_vecVelocity, mv->m_vecVelocity);

		// Con_DPrintf("%f %f: %.0f %.0f %.0f\n", vel, trace.fraction, ent->velocity[0], ent->velocity[1], ent->velocity[2] );

		if (vel < (30 * 30) || (player->GetMoveCollide() != MOVECOLLIDE_FLY_BOUNCE))
		{
			SetGroundEntity(&pm);
			mv->m_vecVelocity.Init();
		}
		else
		{
			VectorScale(mv->m_vecVelocity, (1.0 - pm.fraction) * gpGlobals->frametime * 0.9, move);
			PushEntity(move, &pm);
		}
		VectorSubtract(mv->m_vecVelocity, base, mv->m_vecVelocity);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHorizonGameMovement::FullTossMove(void)
{
	trace_t pm;
	Vector move;

	CheckWater();

	// add velocity if player is moving 
	if ((mv->m_flForwardMove != 0.0f) || (mv->m_flSideMove != 0.0f) || (mv->m_flUpMove != 0.0f))
	{
		Vector forward, right, up;
		float fmove, smove;
		Vector wishdir, wishvel;
		float wishspeed;
		int i;

		/*AngleVectors(mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles
		matrix3x4_t matGravityTransform = GetGravityTransform();
		VectorRotate(forward, matGravityTransform, forward);
		VectorRotate(right, matGravityTransform, right);
		VectorRotate(up, matGravityTransform, up);*/
		player->EyeVectors(&forward, &right, &up);

		// Copy movement amounts
		fmove = mv->m_flForwardMove;
		smove = mv->m_flSideMove;

		VectorNormalize(forward);  // Normalize remainder of vectors.
		VectorNormalize(right);    // 

		for (i = 0; i < 3; i++)       // Determine x and y parts of velocity
			wishvel[i] = forward[i] * fmove + right[i] * smove;

		wishvel += mv->m_flUpMove * GetGravityDirection();

		VectorCopy(wishvel, wishdir);   // Determine maginitude of speed of move
		wishspeed = VectorNormalize(wishdir);

		//
		// Clamp to server defined max speed
		//
		if (wishspeed > mv->m_flMaxSpeed)
		{
			VectorScale(wishvel, mv->m_flMaxSpeed / wishspeed, wishvel);
			wishspeed = mv->m_flMaxSpeed;
		}

		// Set pmove velocity
		Accelerate(wishdir, wishspeed, sv_accelerate.GetFloat());
	}
	
	if (DeterminedVectorLengthUsingAxis(mv->m_vecVelocity) > 0)
	{
		SetGroundEntity(NULL);
	}

	// If on ground and not moving, return.
	if (player->GetGroundEntity() != NULL)
	{
		if (VectorCompare(player->GetBaseVelocity(), vec3_origin) &&
			VectorCompare(mv->m_vecVelocity, vec3_origin))
			return;
	}

	CheckVelocity();

	// add gravity
	if (player->GetMoveType() == MOVETYPE_FLYGRAVITY)
	{
		AddGravity();
	}

	// move origin
	// Base velocity is not properly accounted for since this entity will move again after the bounce without
	// taking it into account
	VectorAdd(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);

	CheckVelocity();

	VectorScale(mv->m_vecVelocity, gpGlobals->frametime, move);
	VectorSubtract(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);

	PushEntity(move, &pm);	// Should this clear basevelocity

	CheckVelocity();

	if (pm.allsolid)
	{
		// entity is trapped in another solid
		SetGroundEntity(&pm);
		mv->m_vecVelocity.Init();
		return;
	}

	if (pm.fraction != 1)
	{
		PerformFlyCollisionResolution(pm, move);
	}

	// check for in water
	CheckWater();
}