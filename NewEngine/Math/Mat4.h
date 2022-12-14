#pragma once
#include "Vec3.h"

class Mat4
{
public:
	float m[4][4] = {};

	Mat4();
	Mat4(
		float _00, float _01, float _02, float _03,
		float _10, float _11, float _12, float _13,
		float _20, float _21, float _22, float _23,
		float _30, float _31, float _32, float _33);

	static Mat4 Zero();		// ゼロ行列を返す関数
	static Mat4 Identity();	// 単位行列を返す関数
	static Mat4 Inverse(const Mat4& mat);	// 逆行列を返す関数

	Vec3 ExtractAxisX() const;
	Vec3 ExtractAxisY() const;
	Vec3 ExtractAxisZ() const;

	// 単項演算子オーバーロード
	Mat4 operator*(const Mat4& other) const;
	Mat4 operator*(const float& num);

	// 代入演算子オーバーロード
	Mat4& operator*=(const Mat4& other);

	// 比較演算子のオーバーロード
	bool operator!=(const Mat4& other) const;

};