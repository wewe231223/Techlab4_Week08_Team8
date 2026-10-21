#pragma once

#include <cmath>
#include <iosfwd>
#include <limits>
#include <type_traits>

#include "Matrix.h"
#include "Vector.h"

// 값을 복사해 보관하며 실수 성분은 numeric_limits의 epsilon으로 비교한다.
template <typename T>
class EpsilonCompare {
public:
	explicit EpsilonCompare(const T& Value);

public:
	bool operator==(const EpsilonCompare& Other) const;
	bool operator!=(const EpsilonCompare& Other) const;

private:
	T mValue{};
};

template <typename T>
EpsilonCompare<T>::EpsilonCompare(const T& Value)
	: mValue{ Value } {
}

template <typename T>
bool EpsilonCompare<T>::operator==(const EpsilonCompare& Other) const {
	if constexpr (std::is_floating_point_v<T>) {
		return std::isfinite(mValue) && std::isfinite(Other.mValue) && std::abs(mValue - Other.mValue) <= std::numeric_limits<T>::epsilon();
	} else {
		return mValue == Other.mValue;
	}
}

template <typename T>
bool EpsilonCompare<T>::operator!=(const EpsilonCompare& Other) const {
	return !(*this == Other);
}

template <>
bool EpsilonCompare<FVector>::operator==(const EpsilonCompare<FVector>& Other) const;

template <>
bool EpsilonCompare<FMatrix>::operator==(const EpsilonCompare<FMatrix>& Other) const;
