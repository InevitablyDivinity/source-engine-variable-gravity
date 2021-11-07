#ifndef HORIZON_GAMEMOVEMENT_H
#define HORIZON_GAMEMOVEMENT_H

#include "gamemovement.h"
#include "horizon_player_shared.h"

class CHorizonGameMovement : public CGameMovement
{
public:
	DECLARE_CLASS(CHorizonGameMovement, CGameMovement);

	CHorizonGameMovement();

	inline Vector GetGravityDirection() const { return ToHorizonPlayer(player)->GetGravityDirection(); }
	// Used to enable the gravity/up/down axis in calculation
	inline Vector GetGravityAxis() const { return ToHorizonPlayer(player)->GetGravityAxis(); }
	// Used to enable the non-gravity axes in calculation
	inline Vector GetGravityAxisInverse() const { return ToHorizonPlayer(player)->GetGravityAxisInverse(); }

	inline matrix3x4_t GetGravityTransform() { return ToHorizonPlayer(player)->GetUpdateGravityTransform(); }
protected:
	bool m_bShouldUpdateMovementBounds;

	// Retrieves the gravity axis' velocity as a scalar
	inline float DeterminedVectorLengthUsingAxis(const Vector& vecVelocity)
	{
		Vector vecGravityVelocity = vecVelocity * GetGravityAxis();
		bool bNegative = vecGravityVelocity.Normalized() == GetGravityDirection();

		float xyz_squared =
			vecGravityVelocity.x * vecGravityVelocity.x +
			vecGravityVelocity.y * vecGravityVelocity.y +
			vecGravityVelocity.z * vecGravityVelocity.z;
		return bNegative ? (vec_t)-FastSqrt(xyz_squared) : (vec_t)FastSqrt(xyz_squared);
	}

	void TracePlayerBBoxForGround(ITraceListData* pTraceListData, const Vector& start,
		const Vector& end, const Vector& minsSrc,
		const Vector& maxsSrc, unsigned int fMask,
		ITraceFilter* filter, trace_t& pm, float minGroundNormalZ, bool overwriteEndpos, int* pCounter);

	virtual void	SetupMovementBounds(CMoveData* move);

	// Does most of the player movement logic.
	// Returns with origin, angles, and velocity modified in place.
	// were contacted during the move.
	virtual void	PlayerMove(void);

	virtual void	CheckWaterJump(void);

	virtual void	WaterMove(void);

	virtual void	WaterJump(void);

	virtual void	AirMove(void);

	// Only used by players.  Moves along the ground when player is a MOVETYPE_WALK.
	virtual void	WalkMove(void);

	// Try to keep a walking player on the ground when running down slopes etc
	virtual void	StayOnGround(void);

	// Handle MOVETYPE_WALK.
	virtual void	FullWalkMove();

	// Implement this if you want to know when the player collides during OnPlayerMove
	virtual void	OnTryPlayerMoveCollision(trace_t& tr) {}


	// Decompoosed gravity
	virtual void	StartGravity(void);
	virtual void	FinishGravity(void);

	// Apply normal ( undecomposed ) gravity
	virtual void	AddGravity(void);

	// Returns true if he started a jump (ie: should he play the jump animation)?
	virtual bool	CheckJumpButton(void);	// Overridden by each game.

	// Dead player flying through air., e.g.
	virtual void    FullTossMove(void);

	// The basic solid body movement clip that slides along multiple planes
	virtual int		TryPlayerMove(Vector* pFirstDest = NULL, trace_t* pFirstTrace = NULL);

	virtual bool	LadderMove(void);
	virtual float	LadderDistance(void) const { return 2.0f; }	///< Returns the distance a player can be from a ladder and still attach to it
	virtual unsigned int LadderMask(void) const { return MASK_PLAYERSOLID; }
	virtual float	ClimbSpeed(void) const { return MAX_CLIMB_SPEED; }
	virtual float	LadderLateralMultiplier(void) const { return 1.0f; }

	// Slide off of the impacting object
	// returns the blocked flags:
	// 0x01 == floor
	// 0x02 == step / wall
	virtual int		ClipVelocity(Vector& in, Vector& normal, Vector& out, float overbounce);

	// Check if the point is in water.
	// Sets refWaterLevel and refWaterType appropriately.
	// If in water, applies current to baseVelocity, and returns true.
	virtual bool	CheckWater(void);
	virtual void	GetWaterCheckPosition(int waterLevel, Vector* pos);

	// Determine if player is in water, on ground, etc.
	virtual void CategorizePosition(void);

	virtual void	CheckFalling(void);

	// Ducking
	virtual void	Duck(void);
	virtual bool	CanUnDuckJump(trace_t& trace);
	virtual void	FinishUnDuckJump(trace_t& trace);
	virtual void	SetDuckedEyeOffset(float duckFraction);
	virtual void	FixPlayerCrouchStuck(bool moveup);

	// Commander view movement
	void			IsometricMove(void);

	virtual void	SetGroundEntity(trace_t* pm);

	virtual void	StepMove(Vector& vecDestination, trace_t& trace);

	// Performs the collision resolution for fliers.
	void			PerformFlyCollisionResolution(trace_t& pm, Vector& move);
};

#endif // HORIZON_GAMEMOVEMENT_H