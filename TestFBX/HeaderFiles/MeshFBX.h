#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>


class MaterialFBX
{
public:
	float emissive[3]; int hasDiffuse;
	float ambient[3]; int hasSpecular;
	float diffuse[3]; float hasNormal;
	float specular[3]; float shininess;

	uint32_t TextureDiffuse;
	uint32_t TextureSpecular;
	uint32_t TextureNormal;	
};

class MeshFBX
{
public:
	uint32_t VBO;
	uint32_t IBO;
	uint32_t VAO;
	uint32_t NumIndices;

	std::vector<MaterialFBX> materials;

	void CreateMesh(const std::vector<float>& vertices,
		const std::vector<uint16_t>& indices);
	void DestroyMesh();
	void Draw();
};



/*
class FbxManager;
class FbxScene;
class FbxNode;
*/
// Necessaire si linkage dynamique (dll)
#define FBXSDK_SHARED
#include "fbxsdk.h"

#ifdef FBXSDK_SHARED
#pragma comment(lib, "libfbxsdk.lib")
#else
#pragma comment(lib, "libfbxsdk-md.lib")
#endif

#include "AnimationCompresserFBX.h"

struct Keyframe
{
	Keyframe()
		: next(nullptr)
	{}

	FbxLongLong frameNum;
	FbxAMatrix globalTransform;
	Keyframe* next;
};


struct Joint
{
	Joint()
		: node(nullptr), animation(nullptr)
	{}

	~Joint()
	{
		while (animation)
		{
			Keyframe* temp = animation->next;
			delete animation;
			animation = temp;
		}
	}
	std::string name;
	int parentIndex;
	FbxAMatrix globalBindPoseInverse;
	Keyframe* animation;
	FbxNode* node;
};

struct Skeleton
{
	std::vector<Joint> m_Joints;
};

struct BlendingIndexWeightPair
{
	unsigned int mBlendingIndex;
	double mBlendingWeight;

	BlendingIndexWeightPair() :
		mBlendingIndex(0),
		mBlendingWeight(0)
	{}
};

struct CtrlPoint
{
	CtrlPoint()
		: x(0.0f),y(0.0f),z(0.0f)
	{
		mBlendingInfo.reserve(4);
	}

	float x;
	float y; 
	float z;
	std::vector<BlendingIndexWeightPair> mBlendingInfo;

};

class LoaderFBX
{
public:
	FbxManager* m_FbxManager;
	FbxScene* m_Scene;
	std::vector<MeshFBX> m_Meshes;
	
	FbxLongLong mAnimationLength;
	std::string mAnimationName;
	FbxAMatrix tug;
	FbxAMatrix thg;
	bool m_HasAnimation;
	Skeleton mSkeleton;
	std::unordered_map<unsigned int, CtrlPoint*> mControlPoints;

	void Initialize();
	void LoadScene(const char* filename);
	void Shutdown();

	void LoadMesh(const FbxNode* node);
	void ProcessNode(const FbxNode* node,const FbxNode* parent);
	void ProcessSkeletonHierarchy(FbxNode* node);
	FbxSurfaceMaterial* HasMaterial(FbxNode* node, FbxMesh* mesh);
	void ProcessSkeletonHierarchyRecursively(FbxNode* node, int index, int parentIndex);
	MaterialFBX ProcessMaterial(FbxNode* node, FbxMesh* mesh, FbxSurfaceMaterial* material);
	void ProcessJointsAndAnimations(FbxNode* node);
	void ProcessGeometry(FbxNode* node);
	void ProcessControlPoints(FbxNode* node);
	unsigned int FindJointIndexUsingName(const std::string& inJointName);
};