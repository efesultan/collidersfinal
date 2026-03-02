// main.cpp — colliders.mll plugin entry point
// Registers: bellCollider, planeCollider (locators) + meshCollider (deformer)

#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MPxDeformerNode.h>
#include <maya/MPxLocatorNode.h>

// ── Forward declarations from each node's translation unit ──────────────
// bellCollider & planeCollider — these are locator nodes (they draw a
// visual gizmo and have no input geometry to deform).
#include "bellCollider.h"
#include "planeCollider.h"

// meshCollider — this IS a deformer: it takes input geometry (the cloth
// mesh) through the inherited "input[].inputGeometry" plug and writes
// to "outputGeometry".
#include "meshCollider.h"

// ── Node IDs ────────────────────────────────────────────────────────────
// Use IDs from the Autodesk-assigned block for production plugins,
// or temporary local IDs (0x00000 – 0x0007ffff) for development.
static const MTypeId bellColliderId(0x0013BA00);
static const MTypeId planeColliderId(0x0013BA01);
static const MTypeId meshColliderId(0x0013BA02);

// ========================================================================
//  initializePlugin
// ========================================================================
MStatus initializePlugin(MObject obj)
{
    MStatus status;
    MFnPlugin fnPlugin(obj, "colliders", "1.0", "Any");

    // ── bellCollider  (locator) ─────────────────────────────────────────
    status = fnPlugin.registerNode(
        "bellCollider",                 // node type name
        bellColliderId,                 // MTypeId
        BellCollider::creator,          // creator function
        BellCollider::initialize,       // initialize function
        MPxNode::kLocatorNode           // <-- locator: correct for this node
    );
    CHECK_MSTATUS_AND_RETURN_IT(status);

    // ── planeCollider (locator) ─────────────────────────────────────────
    status = fnPlugin.registerNode(
        "planeCollider",
        planeColliderId,
        PlaneCollider::creator,
        PlaneCollider::initialize,
        MPxNode::kLocatorNode           // <-- locator: correct for this node
    );
    CHECK_MSTATUS_AND_RETURN_IT(status);

    // ── meshCollider  (DEFORMER) ────────────────────────────────────────
    //
    //  *** THIS WAS THE BUG ***
    //
    //  BROKEN (was):
    //      MPxNode::kLocatorNode
    //
    //  FIXED (now):
    //      MPxNode::kDeformerNode
    //
    //  Why it matters:
    //    • cmds.deformer() queries the registry for kDeformerNode types.
    //    • A kLocatorNode is invisible to that command, so Maya raises:
    //        RuntimeError: "meshCollider" is not a valid deformer type.
    //    • The class MUST also inherit MPxDeformerNode (see meshCollider.h).
    //
    status = fnPlugin.registerNode(
        "meshCollider",
        meshColliderId,
        MeshCollider::creator,
        MeshCollider::initialize,
        MPxNode::kDeformerNode          // <-- FIXED: was kLocatorNode
    );
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MGlobal::displayInfo("colliders.mll loaded  (bell | plane | meshCollider)");
    return MS::kSuccess;
}

// ========================================================================
//  uninitializePlugin
// ========================================================================
MStatus uninitializePlugin(MObject obj)
{
    MStatus status;
    MFnPlugin fnPlugin(obj);

    status = fnPlugin.deregisterNode(bellColliderId);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    status = fnPlugin.deregisterNode(planeColliderId);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    status = fnPlugin.deregisterNode(meshColliderId);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MGlobal::displayInfo("colliders.mll unloaded");
    return MS::kSuccess;
}
