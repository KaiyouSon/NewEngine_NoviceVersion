#pragma once
#include "Vec3.h"

class Random
{
public:
	static void Init();

	static int Range(const int& min, const int& max);			// int型の乱数を取得
	static float RangeF(const float& min, const float& max);	// float型の乱数を取得
	static Vec2 RangeVec2(const int& min, const int& max, const bool& isSameValue = false);		// Vec2型の乱数を取得
	static Vec3 RangeVec3(const int& min, const int& max, const bool& isSameValue = false);		// Vec3型の乱数を取得
	static bool RangeB(const float& rate);						// 確率でtrueを返す
	static float RangeAngle(const float& min = 1.f, const float& max = 360.f);				// ランダムの角度を返す
	static float RangeRadian(const float& min = 0.01745f, const float& max = 6.28319f);		// ランダムのラジアンを返す
	static Vec2 RangeInCircle(const float& minRadius, const float& maxRadius);	// ランダムで円内の点を返す
};