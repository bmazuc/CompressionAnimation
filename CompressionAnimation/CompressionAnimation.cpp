// CompressionAnimation.cpp : définit le point d'entrée pour l'application console.
//

#include "stdafx.h"
#include "AnimationCompresserFBX.h"

int main()
{
	AnimationCompresserFBX* compresser = new AnimationCompresserFBX();

	compresser->Execute();

	delete compresser;
	compresser = nullptr;
    return 0;
}

