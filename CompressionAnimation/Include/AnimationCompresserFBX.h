#pragma once

#define FBXSDK_SHARED
#include "fbxsdk.h"

#ifdef FBXSDK_SHARED
#pragma comment(lib, "libfbxsdk.lib")
#else
#pragma comment(lib, "libfbxsdk-md.lib")
#endif

enum E_COMPRESSIONALGORITHM
{
	NONE = 0,
	SIMPLE_QUANTIZATION,
	ADVANCED_QUANTIZATION,
	SUB_SAMPLING,
	LINEAR_KEY_REDUCTION,
	CURVE_FITTING,
	SIGNAL_PROCESSING
};

class AnimationCompresserFBX
{
public:
	AnimationCompresserFBX();
	AnimationCompresserFBX(AnimationCompresserFBX&& compresser)				= delete;
	AnimationCompresserFBX(AnimationCompresserFBX& compresser)				= delete;
	~AnimationCompresserFBX();

	AnimationCompresserFBX& operator=(AnimationCompresserFBX&& compresser)	= delete;
	AnimationCompresserFBX& operator=(AnimationCompresserFBX& compresser)	= delete;

	void SetScene(FbxScene* scene) { m_Scene = scene; }

	void Execute();

private:
	void ImportScene(const char* filename);
	void ExportScene(const char* filename);

	void ApplyAlgorithmPerKey(const char* filename, E_COMPRESSIONALGORITHM algorithm, bool bCompress = true);
	void ChooseAlgorithm(E_COMPRESSIONALGORITHM algorithm, FbxAnimCurve* curve, int idx, bool bCompress);

	void ApplySimpleQuantization(FbxAnimCurve* curve, int idx, bool bCompress);
	void ApplyLinearKeyReduction(FbxAnimCurve* curve, int idx);

	FbxManager*	m_FbxManager;
	FbxScene*	m_Scene;
};