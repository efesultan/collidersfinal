// meshCollider.h — Mesh-to-mesh cloth collision deformer
//
// KEY REQUIREMENT: Must inherit from MPxDeformerNode so that:
//   1. Maya provides the "input[].inputGeometry" / "outputGeometry" plug
//      wiring automatically.
//   2. The deform() virtual is called per-geometry at evaluation time.
//   3. Registration with kDeformerNode in main.cpp is valid.

#pragma once

#include <maya/MPxDeformerNode.h>  // NOT MPxLocatorNode, NOT MPxNode
#include <maya/MTypeId.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MItGeometry.h>
#include <maya/MMatrix.h>
#include <maya/MPointArray.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>

class MeshCollider : public MPxDeformerNode   // <-- MUST be MPxDeformerNode
{
public:
    MeshCollider() {}
    ~MeshCollider() override {}

    // ── Factory (required by registerNode) ──────────────────────────────
    static void* creator() { return new MeshCollider(); }

    // ── Attribute setup ─────────────────────────────────────────────────
    static MStatus initialize();

    // ── Core deformation entry point ────────────────────────────────────
    //  Maya calls this once per connected geometry per evaluation.
    //    iter         — vertex iterator for the current output geometry
    //    dataBlock    — gives access to all plug values
    //    localToWorld — local-to-world matrix of the driven mesh
    //    geomIndex    — which element of input[] is being evaluated
    MStatus deform(MDataBlock&    dataBlock,
                   MItGeometry&   iter,
                   const MMatrix& localToWorld,
                   unsigned int   geomIndex) override;

    // ── Attributes ──────────────────────────────────────────────────────
    //  colliderMesh:  the body mesh that cloth should not penetrate
    //  collideOffset: push-out padding distance
    static MObject aColliderMesh;     // kMesh typed attribute
    static MObject aCollideOffset;    // float
};
