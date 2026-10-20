#include "EnginePCH.h"
#include "EpsilonCompare.h"

#include "Matrix.h"
#include "Vector.h"

template <>
bool EpsilonCompare<FVector>::operator==(const EpsilonCompare<FVector>& Other) const {
	const EpsilonCompare<float> LeftX{ mValue.X };
	const EpsilonCompare<float> LeftY{ mValue.Y };
	const EpsilonCompare<float> LeftZ{ mValue.Z };

	const EpsilonCompare<float> RightX{ Other.mValue.X };
	const EpsilonCompare<float> RightY{ Other.mValue.Y };
	const EpsilonCompare<float> RightZ{ Other.mValue.Z };

	return LeftX == RightX && LeftY == RightY && LeftZ == RightZ;
}

template <>
bool EpsilonCompare<FMatrix>::operator==(const EpsilonCompare<FMatrix>& Other) const {
	for (uint32 Row{}; Row < 4; ++Row) {
		for (uint32 Column{}; Column < 4; ++Column) {
			const EpsilonCompare<float> Left{ mValue.M[Row][Column] };
			const EpsilonCompare<float> Right{ Other.mValue.M[Row][Column] };

			if (Left != Right) {
				return false;
			}
		}
	}

	return true;
}
