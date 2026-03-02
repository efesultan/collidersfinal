// meshCollider.h — Realtime mesh-to-mesh cloth collision node with VP2 debug drawing
//
// This is a kLocatorNode (NOT a deformer). It:
//   - Takes an input garment mesh and one or more collider meshes
//   - Pushes garment vertices out of colliders via compute()
//   - Outputs the deformed mesh through attr_outputMesh
//   - Draws debug overlays (collider wireframes, contact points) via VP2
//
// FIX: Renamed static "typeId" → "id" to avoid C2761 clash with MPxNode::typeId()
// FIX: Added all missing declarations that meshCollider.cpp requires

#pragma once

#include <vector>

#include <maya/MPxLocatorNode.h>
#include <maya/MTypeId.h>
#include <maya/MString.h>
#include <maya/MDataBlock.h>
#include <maya/MPlug.h>
#include <maya/MMatrix.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MVector.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MIntArray.h>
#include <maya/MColor.h>
#include <maya/MColorArray.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnData.h>
#include <maya/MDagPath.h>

// VP2 draw override headers — these were missing and caused:
//   C2027: undefined type 'MHWRender::MUIDrawManager'
//   C2027: undefined type 'MDagPath'
#include <maya/MDrawRegistry.h>
#include <maya/MPxDrawOverride.h>
#include <maya/MUserData.h>
#include <maya/MDrawContext.h>

// Forward-declare so MeshColliderDrawOverride can reference MeshCollider
class MeshCollider;

// ═════════════════════════════════════════════════════════════════════
//  DrawData — cached viewport data populated in compute(), read in draw
// ═════════════════════════════════════════════════════════════════════

struct MeshColliderDrawData
{
    MPointArray garmentPoints;              // deformed garment (world space)
    MPointArray restPoints;                 // undeformed garment (world space)

    std::vector<int> contactVertexIndices;  // indices of vertices that were pushed

    MColor garmentColor;
    MColor colliderColor;
    MColor contactColor;

    MIntArray triangleCounts;               // garment triangle counts
    MIntArray triangleIndices;              // garment triangle indices

    std::vector<MPointArray> colliderPointsList;        // per-collider world points
    std::vector<MIntArray>   colliderTriCountsList;     // per-collider tri counts
    std::vector<MIntArray>   colliderTriIndicesList;    // per-collider tri indices
};

// ═════════════════════════════════════════════════════════════════════
//  MeshCollider — the locator node
// ═════════════════════════════════════════════════════════════════════

class MeshCollider : public MPxLocatorNode
{
public:
    MeshCollider() {}
    ~MeshCollider() override {}

    static void*   creator() { return new MeshCollider(); }
    static MStatus initialize();

    MStatus compute(const MPlug& plug, MDataBlock& dataBlock) override;

    // Called by the draw override to render debug overlays
    void drawUI(MHWRender::MUIDrawManager& drawManager);

    // ── Node identity ────────────────────────────────────────────────
    //  Named "id" to avoid C2761 clash with virtual MPxNode::typeId().
    static MTypeId id;

    // ── VP2 draw override registration strings ───────────────────────
    static MString drawDbClassification;
    static MString drawRegistrantId;

    // ── Attributes ───────────────────────────────────────────────────
    static MObject attr_inputMesh;
    static MObject attr_inputMeshMatrix;
    static MObject attr_colliderMesh;
    static MObject attr_colliderMatrix;
    static MObject attr_pushOffset;
    static MObject attr_envelope;
    static MObject attr_iterations;
    static MObject attr_useProxyNormal;
    static MObject attr_drawColor;
    static MObject attr_drawOpacity;
    static MObject attr_drawColliders;
    static MObject attr_drawContacts;
    static MObject attr_outputMesh;

    // ── Draw data (written by compute, read by draw override) ────────
    MeshColliderDrawData drawData;
};

// ═════════════════════════════════════════════════════════════════════
//  MeshColliderUserData — carries a pointer to MeshCollider for drawing
// ═════════════════════════════════════════════════════════════════════
//
//  Maya 2026 warns about the old MUserData(bool) constructor.
//  Using the default MUserData() constructor silences C4996.

class MeshColliderUserData : public MHWRender::MUserData
{
public:
    MeshColliderUserData() : MHWRender::MUserData() {}
    ~MeshColliderUserData() override {}

    MeshCollider* meshCollider = nullptr;
};

// ═════════════════════════════════════════════════════════════════════
//  MeshColliderDrawOverride — VP2.0 draw override
// ═════════════════════════════════════════════════════════════════════

class MeshColliderDrawOverride : public MHWRender::MPxDrawOverride
{
public:
    MeshColliderDrawOverride(const MObject& obj)
        : MHWRender::MPxDrawOverride(obj, nullptr, /* isAlwaysDirty = */ true) {}

    ~MeshColliderDrawOverride() override {}

    static MHWRender::MPxDrawOverride* creator(const MObject& obj)
    {
        return new MeshColliderDrawOverride(obj);
    }

    MHWRender::DrawAPI supportedDrawAPIs() const override
    {
        return MHWRender::kAllDevices;
    }

    bool isBounded(const MDagPath&, const MDagPath&) const override
    {
        return false;
    }

    MUserData* prepareForDraw(
        const MDagPath& objPath,
        const MDagPath& cameraPath,
        const MHWRender::MFrameContext& frameContext,
        MUserData* oldData) override;

    void addUIDrawables(
        const MDagPath& objPath,
        MHWRender::MUIDrawManager& drawManager,
        const MHWRender::MFrameContext& frameContext,
        const MUserData* data) override;

    bool hasUIDrawables() const override { return true; }
};
