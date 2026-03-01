#pragma once

#include <maya/MPxLocatorNode.h>
#include <maya/MPxDrawOverride.h>
#include <maya/MDrawRegistry.h>
#include <maya/MPointArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MColorArray.h>
#include <vector>
#include <string>

using namespace std;

struct MeshColliderDrawData_t
{
	MPointArray garmentPoints;
	MPointArray restPoints;
	MIntArray triangleCounts;
	MIntArray triangleIndices;
	vector<MPointArray> colliderPointsList;
	vector<MIntArray> colliderTriCountsList;
	vector<MIntArray> colliderTriIndicesList;
	vector<int> contactVertexIndices;
	MColor garmentColor;
	MColor colliderColor;
	MColor contactColor;
};

class MeshCollider : public MPxLocatorNode
{
public:
	static MTypeId typeId;
	static MString drawDbClassification;
	static MString drawRegistrantId;

	// Input attributes
	static MObject attr_inputMesh;
	static MObject attr_inputMeshMatrix;
	static MObject attr_colliderMesh;
	static MObject attr_colliderMatrix;
	static MObject attr_pushOffset;
	static MObject attr_envelope;
	static MObject attr_iterations;
	static MObject attr_useProxyNormal;

	// Draw attributes
	static MObject attr_drawColor;
	static MObject attr_drawOpacity;
	static MObject attr_drawColliders;
	static MObject attr_drawContacts;

	// Output attributes
	static MObject attr_outputMesh;

	MeshColliderDrawData_t drawData;

	static void* creator() { return new MeshCollider(); }
	static MStatus initialize();
	MStatus compute(const MPlug&, MDataBlock&);
	void drawUI(MHWRender::MUIDrawManager&);

private:
};

class MeshColliderUserData : public MUserData
{
public:
	MeshColliderUserData() : MUserData(false) {}
	MeshCollider* meshCollider{ nullptr };
};

class MeshColliderDrawOverride : public MHWRender::MPxDrawOverride
{
public:
	static MHWRender::MPxDrawOverride* creator(const MObject& obj) { return new MeshColliderDrawOverride(obj); }
	MHWRender::DrawAPI supportedDrawAPIs() const { return MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile; }
	MUserData* prepareForDraw(const MDagPath& objPath, const MDagPath& cameraPath, const MHWRender::MFrameContext& frameContext, MUserData* oldData);
	virtual bool hasUIDrawables() const { return true; }
	virtual void addUIDrawables(const MDagPath& objPath, MHWRender::MUIDrawManager& drawManager, const MHWRender::MFrameContext& frameContext, const MUserData* data);

private:
	MeshColliderDrawOverride(const MObject& obj) : MHWRender::MPxDrawOverride(obj, NULL, true) {}
};
