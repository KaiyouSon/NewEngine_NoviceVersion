#include "Collider.h"

// ----- 円コライダー ------------------- //
CircleCollider::CircleCollider()
{
}

CircleCollider::CircleCollider(const Vec2& centerPos, const float& radius)
{
}

// ----- 球コライダー ------------------- //
SphereCollider::SphereCollider() :
	centerPos(0, 0, 0), radius(1)
{
}
SphereCollider::SphereCollider(const Vec3& centerPos, const float& radius) :
	centerPos(centerPos), radius(radius)
{
}

// ----- レイコライダー ----------------- //
ReyCollider::ReyCollider() :
	startPos(0, 0, 0), dirVec(0, 0, 1)
{
}
ReyCollider::ReyCollider(const Vec3& startPos, const Vec3& dirVec) :
	startPos(startPos), dirVec(dirVec)
{
}

// ----- 線コライダー ------------------- //
LineCollider::LineCollider() :
	startPos(0, 0, 0), endPos(0, 0, 0)
{
}

LineCollider::LineCollider(const Vec3& startPos, const Vec3& endPos) :
	startPos(startPos), endPos(endPos)
{
}

// ----- メッシュコライダー ------------- //
MeshCollider::MeshCollider() :
	centerPos(0, 0, 0),
	upperLeftPos(0, 0, 0), upperRightPos(0, 0, 0),
	lowerLeftPos(0, 0, 0), lowerRightPos(0, 0, 0),
	normal(0, 0, 0)
{
}

MeshCollider::MeshCollider(const Vec3& centerPos, const Vec3& scale) :
	centerPos(centerPos),
	upperLeftPos(centerPos.x - scale.x, centerPos.y + scale.y, centerPos.z),
	upperRightPos(centerPos.x + scale.x, centerPos.y + scale.y, centerPos.z),
	lowerLeftPos(centerPos.x - scale.x, centerPos.y - scale.y, centerPos.z),
	lowerRightPos(centerPos.x + scale.x, centerPos.y - scale.y, centerPos.z),
	normal(Vec3::Cross(
		(lowerRightPos - lowerLeftPos).Norm(),
		(upperLeftPos - lowerLeftPos).Norm()))
{
}