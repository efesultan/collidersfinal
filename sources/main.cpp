// main.cpp — colliders.mll plugin entry point
//
// All three nodes are kLocatorNode with VP2 draw overrides:
//   bellCollider   — bell-shaped collision solver + debug drawing
//   planeCollider  — plane collision solver + debug drawing
//   meshCollider   — mesh-to-mesh collision solver + debug drawing
//
// Uses class static IDs and draw registration strings to avoid mismatches.

#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MDrawRegistry.h>

#include "bellCollider.h"
#include "planeCollider.h"
#include "meshCollider.h"

MStatus initializePlugin(MObject obj)
{
	MStatus status;
	MFnPlugin fnPlugin(obj, "Colliders", "1.0", "Any");

	// ── bellCollider ─────────────────────────────────────────────────

	status = fnPlugin.registerNode(
		"bellCollider",
		BellCollider::typeId,
		BellCollider::creator,
		BellCollider::initialize,
		MPxNode::kLocatorNode,
		&BellCollider::drawDbClassification);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	status = MHWRender::MDrawRegistry::registerDrawOverrideCreator(
		BellCollider::drawDbClassification,
		BellCollider::drawRegistrantId,
		BellColliderDrawOverride::creator);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	// ── planeCollider ────────────────────────────────────────────────

	status = fnPlugin.registerNode(
		"planeCollider",
		PlaneCollider::typeId,
		PlaneCollider::creator,
		PlaneCollider::initialize,
		MPxNode::kLocatorNode,
		&PlaneCollider::drawDbClassification);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	status = MHWRender::MDrawRegistry::registerDrawOverrideCreator(
		PlaneCollider::drawDbClassification,
		PlaneCollider::drawRegistrantId,
		PlaneColliderDrawOverride::creator);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	// ── meshCollider ─────────────────────────────────────────────────
	//  Uses MeshCollider::id (not ::typeId — renamed to avoid C2761)

	status = fnPlugin.registerNode(
		"meshCollider",
		MeshCollider::id,
		MeshCollider::creator,
		MeshCollider::initialize,
		MPxNode::kLocatorNode,
		&MeshCollider::drawDbClassification);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	status = MHWRender::MDrawRegistry::registerDrawOverrideCreator(
		MeshCollider::drawDbClassification,
		MeshCollider::drawRegistrantId,
		MeshColliderDrawOverride::creator);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	MGlobal::displayInfo("colliders.mll loaded  (bell | plane | meshCollider)");
	return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj)
{
	MStatus status;
	MFnPlugin fnPlugin(obj);

	// ── bellCollider ─────────────────────────────────────────────────

	status = MHWRender::MDrawRegistry::deregisterDrawOverrideCreator(
		BellCollider::drawDbClassification,
		BellCollider::drawRegistrantId);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	status = fnPlugin.deregisterNode(BellCollider::typeId);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	// ── planeCollider ────────────────────────────────────────────────

	status = MHWRender::MDrawRegistry::deregisterDrawOverrideCreator(
		PlaneCollider::drawDbClassification,
		PlaneCollider::drawRegistrantId);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	status = fnPlugin.deregisterNode(PlaneCollider::typeId);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	// ── meshCollider ─────────────────────────────────────────────────

	status = MHWRender::MDrawRegistry::deregisterDrawOverrideCreator(
		MeshCollider::drawDbClassification,
		MeshCollider::drawRegistrantId);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	status = fnPlugin.deregisterNode(MeshCollider::id);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	MGlobal::displayInfo("colliders.mll unloaded");
	return MS::kSuccess;
}
