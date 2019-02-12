#include <iostream>

#include "MeshFBX.h"
#include "glew.h"
#include <windows.h>
#include <iostream>

extern GLuint LoadAndCreateTexture(const char* fullpath, bool linear = false);

void LoaderFBX::Shutdown()
{
	m_Scene->Destroy();
	m_FbxManager->Destroy();
}

void LoaderFBX::Initialize()
{
	m_HasAnimation = true;
	// 1. Creation Singleton
	m_FbxManager = FbxManager::Create();

	// 2. Initialisation des I/O
	FbxIOSettings* ioSettings =
		FbxIOSettings::Create(m_FbxManager, IOSROOT);
	m_FbxManager->SetIOSettings(ioSettings);
}

void LoaderFBX::LoadScene(const char* name)
{
	// 1. Creation de la scene (stockage)
	m_Scene = FbxScene::Create(m_FbxManager, name);
	// 2. Importation 
	FbxImporter *importer = FbxImporter::Create(
		m_FbxManager, "");
	// 3. lecture du fichier fbx
	bool status = importer->Initialize(name, -1,
		m_FbxManager->GetIOSettings());
	// 4. copie du contenu dans notre scene
	status = importer->Import(m_Scene);
	// 5. l'importer n'est plus utile a ce stade
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

	//FbxAnimStack* currAnimStack = m_Scene->GetSrcObject<FbxAnimStack>(0);
	//if (currAnimStack)
	//{
	//	FbxAnimStack* currAnimStack = m_Scene->GetSrcObject<FbxAnimStack>(0);
	//	FbxString animStackName = currAnimStack->GetName();
	//	char* mAnimationName = animStackName.Buffer();
	//	FbxTakeInfo* takeInfo = m_Scene->GetTakeInfo(animStackName);
	//	FbxTime start = takeInfo->mLocalTimeSpan.GetStart();
	//	FbxTime end = takeInfo->mLocalTimeSpan.GetStop();
	//}

	ProcessSkeletonHierarchy(root);
	if (mSkeleton.m_Joints.empty())
	{
		m_HasAnimation = false;
	}

	ProcessGeometry(root);
}

void LoaderFBX::ProcessNode(const FbxNode* node,
	const FbxNode* parent) 
{
	const FbxNodeAttribute* att = node->GetNodeAttribute();
	if (att) {
		FbxNodeAttribute::EType type;
		type = att->GetAttributeType();
		switch (type)
		{
		case FbxNodeAttribute::eMesh:
		{
			std::cout << "Mesh BONE " << std::endl;
			LoadMesh(node);
		}
			break;
		case FbxNodeAttribute::eCamera:
			break;
		case FbxNodeAttribute::eLight:
			break;
		default: break;
		}
	}

	int childCount = node->GetChildCount();
	for (int i = 0; i < childCount; i++) {
		const FbxNode* child = node->GetChild(i);
		ProcessNode(child, node);
	}
}

void LoaderFBX::ProcessSkeletonHierarchy(FbxNode * node)
{
	int childCount = node->GetChildCount();
	for (int i = 0; i < childCount; ++i)
	{
		FbxNode* child = node->GetChild(i);
		ProcessSkeletonHierarchyRecursively(child, 0, -1);
	}
}

FbxSurfaceMaterial* LoaderFBX::HasMaterial(FbxNode* node, FbxMesh* mesh)
{
	FbxLayerElementMaterial* matLayerElement = mesh->GetElementMaterial(0);
	if (matLayerElement->GetMappingMode() == FbxLayerElement::eAllSame)
	{
		int indexMat = matLayerElement->GetIndexArray()[0];
		FbxSurfaceMaterial* material = node->GetMaterial(0);
		if (material == nullptr)
		{
			return nullptr;
		}
		else
			return material;
	}
}

void LoaderFBX::ProcessSkeletonHierarchyRecursively(FbxNode * node, int index, int parentIndex)
{
	if (node->GetNodeAttribute() && node->GetNodeAttribute()->GetAttributeType() && node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
	{
		Joint currJoint;
		currJoint.parentIndex = parentIndex;
		currJoint.name = node->GetName();
		mSkeleton.m_Joints.push_back(currJoint);
	}
	int childCount = node->GetChildCount();
	for (int i = 0; i < childCount; i++)
	{
		ProcessSkeletonHierarchyRecursively(node->GetChild(i), mSkeleton.m_Joints.size(), index);
	}

}

MaterialFBX LoaderFBX::ProcessMaterial(FbxNode * node, FbxMesh * mesh, FbxSurfaceMaterial * material)
{

	MaterialFBX meshMaterial;
	FbxString type = material->ShadingModel.Get();

	const FbxProperty propertyDiffuse = material->FindProperty(FbxSurfaceMaterial::sDiffuse);

	if (propertyDiffuse.IsValid())
	{
		FbxDouble3 color = propertyDiffuse.Get<FbxDouble3>();

		const FbxProperty factorDiffuse = material->FindProperty(FbxSurfaceMaterial::sDiffuseFactor);
		if (factorDiffuse.IsValid())
		{
			double factor = factorDiffuse.Get<double>();
			color[0] *= factor; color[1] *= factor; color[2] *= factor;
		}

		int textureCount = propertyDiffuse.GetSrcObjectCount<FbxFileTexture>();
		FbxFileTexture* texture = propertyDiffuse.GetSrcObject<FbxFileTexture>(0);
		if (texture)
		{
			const char* filename = texture->GetFileName();
			meshMaterial.TextureDiffuse = LoadAndCreateTexture(filename, true);
			meshMaterial.hasDiffuse = true;
		}

		meshMaterial.diffuse[0] = color[0]; meshMaterial.diffuse[1] = color[1];
		meshMaterial.diffuse[2] = color[2];
	}

	const FbxProperty propertySpecular = material->FindProperty(FbxSurfaceMaterial::sSpecular);
	if (propertySpecular.IsValid())
	{
		FbxDouble3 color = propertySpecular.Get<FbxDouble3>();

		const FbxProperty factorSpecular =
			material->FindProperty(FbxSurfaceMaterial::sSpecularFactor);
		if (factorSpecular.IsValid()) {
			double factor = factorSpecular.Get<double>();
			color[0] *= factor; color[1] *= factor; color[2] *= factor;
		}

		int textureCount = propertySpecular.GetSrcObjectCount<FbxFileTexture>();
		FbxFileTexture* texture = propertySpecular.GetSrcObject<FbxFileTexture>(0);
		if (texture) {
			const char* filename = texture->GetFileName();
			meshMaterial.TextureSpecular = LoadAndCreateTexture(filename, true);
			meshMaterial.hasSpecular = true;
		}
		meshMaterial.specular[0] = color[0]; meshMaterial.specular[1] = color[1];
		meshMaterial.specular[2] = color[2];
	}

	const FbxProperty propertyBump =
		material->FindProperty(FbxSurfaceMaterial::sBump);
	if (propertyBump.IsValid()) {
		const FbxProperty factorBump =
			material->FindProperty(FbxSurfaceMaterial::sBumpFactor);
		if (factorBump.IsValid()) {
			double factor = factorBump.Get<double>();
			// TODO: stocker ce facteur de scale du bump
		}

		int textureCount = propertyBump.GetSrcObjectCount<FbxFileTexture>();
		FbxFileTexture* texture = propertyBump.GetSrcObject<FbxFileTexture>(0);
		if (texture) {
			const char* filename = texture->GetFileName();
			meshMaterial.TextureNormal = LoadAndCreateTexture(filename);
			meshMaterial.hasNormal = true;
		}
	}

	return meshMaterial;
}

void LoaderFBX::ProcessJointsAndAnimations(FbxNode* node)
{
	FbxMesh* currMesh = node->GetMesh();
	unsigned int numOfDeformers = currMesh->GetDeformerCount();
	// This geometry transform is something I cannot understand
	// I think it is from MotionBuilder
	// If you are using Maya for your models, 99% this is just an
	// identity matrix
	// But I am taking it into account anyways......

	const FbxVector4 lT = node->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 lR = node->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxVector4 lS = node->GetGeometricScaling(FbxNode::eSourcePivot);
	FbxAMatrix geometryTransform = FbxAMatrix(lT, lR, lS);

	// A deformer is a FBX thing, which contains some clusters
	// A cluster contains a link, which is basically a joint
	// Normally, there is only one deformer in a mesh
	for (unsigned int deformerIndex = 0; deformerIndex < numOfDeformers; ++deformerIndex)
	{
		// There are many types of deformers in Maya,
		// We are using only skins, so we see if this is a skin
		FbxSkin* currSkin = reinterpret_cast<FbxSkin*>(currMesh->GetDeformer(deformerIndex, FbxDeformer::eSkin));
		if (!currSkin)
		{
			continue;
		}

		unsigned int numOfClusters = currSkin->GetClusterCount();
		for (unsigned int clusterIndex = 0; clusterIndex < numOfClusters; ++clusterIndex)
		{
			FbxCluster* currCluster = currSkin->GetCluster(clusterIndex);
			std::string currJointName = currCluster->GetLink()->GetName();
			unsigned int currJointIndex = FindJointIndexUsingName(currJointName);
			FbxAMatrix transformMatrix;
			FbxAMatrix transformLinkMatrix;
			FbxAMatrix globalBindposeInverseMatrix;

			currCluster->GetTransformMatrix(transformMatrix);	// The transformation of the mesh at binding time
			currCluster->GetTransformLinkMatrix(transformLinkMatrix);	// The transformation of the cluster(joint) at binding time from joint space to world space
			globalBindposeInverseMatrix = transformLinkMatrix.Inverse() * transformMatrix * geometryTransform;

			// Update the information in mSkeleton 
			mSkeleton.m_Joints[currJointIndex].globalBindPoseInverse = globalBindposeInverseMatrix;
			mSkeleton.m_Joints[currJointIndex].node = currCluster->GetLink();

			// Associate each joint with the control points it affects
			unsigned int numOfIndices = currCluster->GetControlPointIndicesCount();
			int value;
			for (unsigned int i = 0; i < numOfIndices; ++i)
			{
				BlendingIndexWeightPair currBlendingIndexWeightPair;
				currBlendingIndexWeightPair.mBlendingIndex = currJointIndex;
				currBlendingIndexWeightPair.mBlendingWeight = currCluster->GetControlPointWeights()[i];
				//std::cout << "JointName" << currJointName.c_str() << " indices : " << i << "currJointIndex" << currCluster->GetControlPointWeights()[i] << std::endl;
				value = currCluster->GetControlPointIndices()[i];
 				mControlPoints[currCluster->GetControlPointIndices()[i]]->mBlendingInfo.push_back(currBlendingIndexWeightPair);

			}

			// Get animation information
			// Now only supports one take
			FbxAnimStack* currAnimStack = m_Scene->GetSrcObject<FbxAnimStack>(0);
			FbxString animStackName = currAnimStack->GetName();
			mAnimationName = animStackName.Buffer();
			FbxTakeInfo* takeInfo = m_Scene->GetTakeInfo(animStackName);
			
			FbxTime start = takeInfo->mLocalTimeSpan.GetStart();
			FbxTime end = takeInfo->mLocalTimeSpan.GetStop();
			mAnimationLength = end.GetFrameCount(FbxTime::eFrames24) - start.GetFrameCount(FbxTime::eFrames24) + 1;
			Keyframe** currAnim = &mSkeleton.m_Joints[currJointIndex].animation;

			for (FbxLongLong i = start.GetFrameCount(FbxTime::eFrames24); i <= end.GetFrameCount(FbxTime::eFrames24); ++i)
			{
				FbxTime currTime;
				currTime.SetFrame(i, FbxTime::eFrames24);
				*currAnim = new Keyframe();
				(*currAnim)->frameNum = i;
				FbxAMatrix currentTransformOffset = node->EvaluateGlobalTransform(currTime) * geometryTransform;
				 
				(*currAnim)->globalTransform = currentTransformOffset.Inverse() * currCluster->GetLink()->EvaluateGlobalTransform(currTime);
				currAnim = &((*currAnim)->next);
			}
		}
	}

	// Some of the control points only have less than 4 joints
	// affecting them.
	// For a normal renderer, there are usually 4 joints
	// I am adding more dummy joints if there isn't enough
	BlendingIndexWeightPair currBlendingIndexWeightPair;
	currBlendingIndexWeightPair.mBlendingIndex = 0;
	currBlendingIndexWeightPair.mBlendingWeight = 0;
	for (auto itr = mControlPoints.begin(); itr != mControlPoints.end(); ++itr)
	{
		for (unsigned int i = itr->second->mBlendingInfo.size(); i <= 4; ++i)
		{
			itr->second->mBlendingInfo.push_back(currBlendingIndexWeightPair);
		}
	}

}

void LoaderFBX::ProcessGeometry(FbxNode * node)
{
	if (node->GetNodeAttribute())
	{
		switch (node->GetNodeAttribute()->GetAttributeType())
		{
		case FbxNodeAttribute::eMesh:
			ProcessControlPoints(node);
			if (m_HasAnimation)
			{
				ProcessJointsAndAnimations(node);
			}
			ProcessNode(node,nullptr);
		//	ProcessMesh(node);
		//	AssociateMaterialToMesh(node);
		//	ProcessMaterials(node);
			break;
		}
	}

	for (int i = 0; i < node->GetChildCount(); ++i)
	{
		ProcessGeometry(node->GetChild(i));
	}
}

void LoaderFBX::ProcessControlPoints(FbxNode * node)
{
	FbxMesh* currMesh = node->GetMesh();
	unsigned int ctrlPointCount = currMesh->GetControlPointsCount();
	for (unsigned int i = 0; i < ctrlPointCount; ++i)
	{
		CtrlPoint* currCtrlPoint = new CtrlPoint();
		
		currCtrlPoint->x = static_cast<float>(currMesh->GetControlPointAt(i).mData[0]);
		currCtrlPoint->y = static_cast<float>(currMesh->GetControlPointAt(i).mData[1]);
		currCtrlPoint->z = static_cast<float>(currMesh->GetControlPointAt(i).mData[2]);
		mControlPoints[i] = currCtrlPoint;
	}
}

unsigned int LoaderFBX::FindJointIndexUsingName(const std::string & inJointName)
{
	for (unsigned int i = 0; i < mSkeleton.m_Joints.size(); ++i)
	{
		if (mSkeleton.m_Joints[i].name == inJointName)
		{
			return i;
		}
	}

	throw std::exception("Skeleton information in FBX file is corrupted.");
}

void LoaderFBX::LoadMesh(const FbxNode* node)
{	
	FbxNode* mutable_node = const_cast<FbxNode*>(node);
	FbxMesh* mesh = mutable_node->GetMesh();
	
	// determine la presence de tangentes
	FbxLayerElementTangent* meshTangents = mesh->GetElementTangent(0);
	if (meshTangents == nullptr) {
		// sinon on genere des tangentes (pour le tangent space normal mapping)
		bool test = mesh->GenerateTangentsDataForAllUVSets(true);
		meshTangents = mesh->GetElementTangent(0);
	}
	FbxLayerElement::EMappingMode tangentMode = meshTangents->GetMappingMode();
	FbxLayerElement::EReferenceMode tangentRefMode = meshTangents->GetReferenceMode();


	FbxSurfaceMaterial* material = nullptr;
	//material = HasMaterial(node, mesh);
	material = HasMaterial(mutable_node, mesh);
	bool hasMaterial = false;
	
	MaterialFBX meshMaterial;
	if (material != nullptr)
	{
		hasMaterial = true;
		meshMaterial = ProcessMaterial(mutable_node, mesh, material);
	}

	// Transform placant l'objet dans la scene.
	// Passer par l'Animation Evaluator (global) accumule les transform des parents
	//FbxAMatrix globalTransform = node->GetScene()->GetAnimationEvaluator()->GetNodeLocalTransform(mutable_node);
	FbxAMatrix globalTransform = node->GetScene()->GetAnimationEvaluator()->GetNodeGlobalTransform(mutable_node);

	// Matrice geometrique locale.
	FbxVector4 translation = node->GetGeometricTranslation(FbxNode::eSourcePivot);
	FbxVector4 rotation = node->GetGeometricRotation(FbxNode::eSourcePivot);
	FbxVector4 scale = node->GetGeometricScaling(FbxNode::eSourcePivot);
	
	FbxAMatrix geometryTransform;
	geometryTransform.SetTRS(translation, rotation, scale);

	FbxAMatrix finalGlobalTransform = globalTransform * geometryTransform;

	std::vector<float> vertices;

	// lecture des indices/elements
	auto IndexCount = mesh->GetPolygonVertexCount();
	std::vector<uint16_t> indices;
	//indices.reserve(IndexCount);

	// recupere la liste des canaux UV (texcoords)
	FbxStringList UVChannelNames;
	mesh->GetUVSetNames(UVChannelNames);
	auto UVChannelCount = UVChannelNames.GetCount();

	auto PolyCount = mesh->GetPolygonCount();
	// methode 1: duplication des attributs
	indices.reserve(PolyCount * 3);

	for (auto poly = 0; poly < PolyCount; poly++) {
		// faces (triangles)
		for (auto i = 0; i < 3; i++) {
			// elements
			auto index = mesh->GetPolygonVertex(poly, i);
			
			// methode1 - duplication des attributs
			indices.emplace_back((uint16_t)(poly*3+i));

			// attributs: 
			// position (x,y,z)
			FbxVector4 point = mesh->GetControlPointAt(index);
			//point = finalGlobalTransform.MultT(point);
			vertices.push_back(point.mData[0]);
			vertices.push_back(point.mData[1]);
			vertices.push_back(point.mData[2]);
			
			// normal (x,y,z)
			FbxVector4 normal;
			mesh->GetPolygonVertexNormal(poly, i, normal);
			normal = finalGlobalTransform.MultT(normal);

			vertices.push_back(normal.mData[0]);
			vertices.push_back(normal.mData[1]);
			vertices.push_back(normal.mData[2]);
			
			// tex coords (s,t)
			auto uv_channel = UVChannelNames.GetStringAt(0);
			FbxVector2 texcoords;
			bool isUnMapped = false;
			bool hasUV = mesh->GetPolygonVertexUV(poly, i, uv_channel, texcoords, isUnMapped);
			vertices.push_back(texcoords.mData[0]);
			vertices.push_back(texcoords.mData[1]);
		
			// tangents (vec3)
			FbxVector4 tangent;
			if (tangentRefMode == FbxLayerElement::eDirect) {
				tangent = meshTangents->GetDirectArray().GetAt(index);
			}
			else //if (tangentRefMode == FbxLayerElement::eIndexToDirect) 
			{
				int indirectIndex = meshTangents->GetIndexArray().GetAt(index);
				tangent = meshTangents->GetDirectArray().GetAt(indirectIndex);
			}
			vertices.push_back(tangent.mData[0]);
			vertices.push_back(tangent.mData[1]);
			vertices.push_back(tangent.mData[2]);

			CtrlPoint* currentPoint = mControlPoints[index];
			for (int i = 0; i < 4; i++)
			{
				vertices.push_back(currentPoint->mBlendingInfo[i].mBlendingIndex);
			}
			for (int i = 0; i < 4; i++)
			{
				vertices.push_back(currentPoint->mBlendingInfo[i].mBlendingWeight);
			}
		}
	}

	MeshFBX meshObject;
	if(hasMaterial == true)
		meshObject.materials.push_back(meshMaterial);
	meshObject.CreateMesh(vertices, indices);
	m_Meshes.push_back(meshObject);
}

void MeshFBX::CreateMesh(
	const std::vector<float>& vertices,
	const std::vector<uint16_t>& indices)
{
	NumIndices = indices.size();

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);	

	glGenBuffers(1, &VBO);
	// notez la relation entre _ARRAY_ et DrawArrays
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER
		, vertices.size() * sizeof(float), vertices.data()
		, GL_STATIC_DRAW);

	glGenBuffers(1, &IBO);
	// notez la relation entre _ELEMENT_ARRAY_ et DrawElements
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER
		, NumIndices * sizeof(uint16_t), indices.data()
		, GL_STATIC_DRAW);

	int stride = 19 * sizeof(float);
	// positions
	glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, 0);
	glEnableVertexAttribArray(0);
	// normals
	glVertexAttribPointer(1, 3, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	// texcoords
	glVertexAttribPointer(2, 2, GL_FLOAT, false, stride, (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);
	// tangents
	glVertexAttribPointer(3, 3, GL_FLOAT, false, stride, (void*)(8 * sizeof(float)));
	glEnableVertexAttribArray(3);

	glVertexAttribPointer(4, 4, GL_FLOAT, false, stride, (void*)(11 * sizeof(float)));
	glEnableVertexAttribArray(4);
	
	glVertexAttribPointer(5, 4, GL_FLOAT, false, stride, (void*)(15 * sizeof(float)));
	glEnableVertexAttribArray(5);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void MeshFBX::DestroyMesh()
{
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &IBO);

	for (auto& mat : materials) {
		if (mat.TextureDiffuse)
			glDeleteTextures(1, &mat.TextureDiffuse);
		if (mat.TextureNormal)
			glDeleteTextures(1, &mat.TextureNormal);
		if (mat.TextureSpecular)
			glDeleteTextures(1, &mat.TextureSpecular);
	}
}

void MeshFBX::Draw()
{
	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, NumIndices, GL_UNSIGNED_SHORT, nullptr);
}