#include "stdafx.h"
#include "AnimationCompresserFBX.h"

#include <windows.h>
#include <iostream>
#include <string>
#include <cstdint>
#include <math.h>

AnimationCompresserFBX::AnimationCompresserFBX()
{
	m_FbxManager = FbxManager::Create();

	// 2. Initialisation des I/O
	FbxIOSettings* ioSettings =
		FbxIOSettings::Create(m_FbxManager, IOSROOT);
	m_FbxManager->SetIOSettings(ioSettings);
}

AnimationCompresserFBX::~AnimationCompresserFBX()
{
	m_Scene->Destroy();
	m_FbxManager->Destroy();
}

void AnimationCompresserFBX::Execute()
{
	char c;
	std::string str;
	int compressionId;

	do
	{
		std::cout << "Enter file name. He must be in Assets file." << std::endl;
		std::cin >> str;

		ImportScene(("../Assets/" + str + ".FBX").c_str());

		std::cout << "Choose Algorithm. 0 : None,\n 1 : Simple Quantization,\n 2 : Linear Key Reduction\n" << std::endl;

		std::cin >> compressionId;

		switch (compressionId)
		{
		case 0: ApplyAlgorithmPerKey(("../Assets/" + str + "NoCompression.FBX").c_str(), E_COMPRESSIONALGORITHM::NONE); break;
		case 1: ApplyAlgorithmPerKey(("../Assets/" + str + "SimpleQuantizationCompression.FBX").c_str(), E_COMPRESSIONALGORITHM::SIMPLE_QUANTIZATION);
				ApplyAlgorithmPerKey(("../Assets/" + str + "SimpleQuantizationDecompression.FBX").c_str(), E_COMPRESSIONALGORITHM::SIMPLE_QUANTIZATION, false); break;
		case 2: ApplyAlgorithmPerKey(("../Assets/" + str + "LinearKeyReductionCompression.FBX").c_str(), E_COMPRESSIONALGORITHM::LINEAR_KEY_REDUCTION); break;
		default: break;
		}

		std::cout << "Do another compression?\n y for yes, n for no." << std::endl;
		std::cin >> c;
	} while (c != 'n' && c != 'N');
}

void AnimationCompresserFBX::ImportScene(const char* filename)
{
	const char* f = filename;
	std::cout << f << std::endl;
	m_Scene = FbxScene::Create(m_FbxManager, f);
	FbxImporter *importer = FbxImporter::Create(m_FbxManager, "");
	bool status = importer->Initialize(f, -1, m_FbxManager->GetIOSettings());
	status = importer->Import(m_Scene);
	importer->Destroy();

	FbxNode* root = m_Scene->GetRootNode();
	// Modifie les parametres geometriques du Root Node
	// 5 - changement de repere
	FbxAxisSystem SceneAxisSystem = m_Scene->GetGlobalSettings().GetAxisSystem();
	FbxAxisSystem OurAxisSystem(FbxAxisSystem::eYAxis, FbxAxisSystem::eParityOdd,
		FbxAxisSystem::eRightHanded);
	if (SceneAxisSystem != OurAxisSystem) {
		OurAxisSystem.ConvertScene(m_Scene);
	}
	// 6 - Changement d'unites (vers cm) : si (ScaleFactor - 1cm) > 0 -> pas des cm
	FbxSystemUnit SceneSystemUnit = m_Scene->GetGlobalSettings().GetSystemUnit();
	if (fabs(SceneSystemUnit.GetScaleFactor() - 1.0) > 0.00001) {
		FbxSystemUnit OurSystemUnit(1.0);
		OurSystemUnit.ConvertScene(m_Scene);
	}

	FbxGeometryConverter lGeomConverter(m_FbxManager);
	lGeomConverter.Triangulate(m_Scene, /*replace*/true);
}

void AnimationCompresserFBX::ExportScene(const char* filename)
{
	//Create an exporter.
	FbxExporter* exporter = FbxExporter::Create(m_FbxManager, "");

	// Initialize the exporter.
	bool status = exporter->Initialize(filename, -1, m_FbxManager->GetIOSettings());
	status = exporter->Export(m_Scene);
	exporter->Destroy();
}

void AnimationCompresserFBX::ApplyAlgorithmPerKey(const char* filename, E_COMPRESSIONALGORITHM algorithm, bool bCompress)
{
	std::cout << filename << std::endl;
	if (m_Scene && algorithm != E_COMPRESSIONALGORITHM::NONE)
	{
		for (int idx = 0; idx < m_Scene->GetSrcObjectCount<FbxAnimStack>(); ++idx)
		{
			FbxAnimStack* animStack = m_Scene->GetSrcObject<FbxAnimStack>(idx);
			std::cout << "Stack Name : " << animStack->GetName() << std::endl;
			animStack->SetName("test");

			for (int idx = 0; idx < animStack->GetSrcObjectCount<FbxAnimLayer>(); ++idx)
			{
				FbxAnimLayer* animLayer = animStack->GetSrcObject<FbxAnimLayer>(idx);
				std::cout << "-Layer Name : " << animLayer->GetName() << std::endl;

				for (int idx = 0; idx < animLayer->GetSrcObjectCount<FbxAnimCurveNode>(); ++idx)
				{
					FbxAnimCurveNode* curveNode = animLayer->GetSrcObject<FbxAnimCurveNode>(idx);
					std::cout << "--Curve Node Name : " << curveNode->GetName() << std::endl;

					for (int idx = 0; idx < curveNode->GetChannelsCount(); ++idx)
					{
						FbxAnimCurve* curve = curveNode->GetCurve(idx);
						if (curve)
						{
							std::cout << "---Curve Name : " << curve->GetName() << std::endl;
							for (int idx = 0; idx < curve->KeyGetCount(); ++idx)
								if (curveNode->GetName() != "T")
									ChooseAlgorithm(algorithm, curve, idx, bCompress);
							Sleep(1000);
						}
					}
				}
			}
		}
	}

	ExportScene(filename);
}

void AnimationCompresserFBX::ChooseAlgorithm(E_COMPRESSIONALGORITHM algorithm, FbxAnimCurve* curve, int idx, bool bCompress)
{
	switch (algorithm)
	{
		case E_COMPRESSIONALGORITHM::SIMPLE_QUANTIZATION:
			ApplySimpleQuantization(curve, idx, bCompress);
			break;
		case E_COMPRESSIONALGORITHM::LINEAR_KEY_REDUCTION:
			if (bCompress)
				ApplyLinearKeyReduction(curve, idx);
			break;
		default: break;
	};
}

void AnimationCompresserFBX::ApplySimpleQuantization(FbxAnimCurve* curve, int idx, bool bCompress)
{
	float value = curve->KeyGetValue(idx);
	
	if (bCompress)
	{
		float normalizedValue = value / 360;
		float scaledValue = normalizedValue * INT16_MAX;
		int16_t quantizedValue = floorf(scaledValue + 0.5);

		curve->KeyModifyBegin();
		curve->KeySetValue(idx, quantizedValue);
		curve->KeyModifyEnd();
	}
	else
	{
		float normalizedValue = value / INT16_MAX;
		float dequantizedValue = 360 * normalizedValue;

		curve->KeyModifyBegin();
		curve->KeySetValue(idx, dequantizedValue);
		curve->KeyModifyEnd();
	}
}

void AnimationCompresserFBX::ApplyLinearKeyReduction(FbxAnimCurve* curve, int idx)
{
	if (idx == 0 || idx == curve->KeyGetCount())
		return;

	int sampleTime = curve->KeyGetTime(idx).GetSecondCount();
	int previousTime = curve->KeyGetTime(idx - 1).GetSecondCount();
	int nextTime = curve->KeyGetTime(idx + 1).GetSecondCount();

	if (nextTime - previousTime == 0)
		return;

	int alpha = (sampleTime - previousTime) / (nextTime - previousTime);

	float value = curve->KeyGetValue(idx - 1) + alpha * (curve->KeyGetValue(idx + 1) - curve->KeyGetValue(idx - 1)); //Lerp

	if (value - curve->KeyGetValue(idx) > -0.000000000000005 && value - curve->KeyGetValue(idx) < 0.000000000000005)
		curve->KeyRemove(idx);
}