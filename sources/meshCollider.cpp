#include <set>
#include <map>

#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MIntArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnDependencyNode.h>

#include <maya/MTransformationMatrix.h>
#include <maya/MQuaternion.h>
#include <maya/MEulerRotation.h>

#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MMeshIntersector.h>

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnGenericAttribute.h>

#include <tbb/parallel_for.h>

#include "meshCollider.h"
#include "utils.hpp"

using namespace std;

// Use a unique typeId — must not clash with bellCollider (1274434) or planeCollider
MTypeId MeshCollider::typeId(1274436);

MObject MeshCollider::attr_inputMesh;
MObject MeshCollider::attr_colliderMesh;
MObject MeshCollider::attr_pushOffset;
MObject MeshCollider::attr_envelope;
MObject MeshCollider::attr_iterations;
MObject MeshCollider::attr_useProxyNormal;
MObject MeshCollider::attr_drawColor;
MObject MeshCollider::attr_drawOpacity;
MObject MeshCollider::attr_drawColliders;
MObject MeshCollider::attr_drawContacts;
MObject MeshCollider::attr_outputMesh;

MString MeshCollider::drawDbClassification = "drawdb/geometry/meshCollider";
MString MeshCollider::drawRegistrantId = "collidersPlugin_meshCollider";

MStatus MeshCollider::compute(const MPlug& plug, MDataBlock& dataBlock)
{
	if (plug != attr_outputMesh)
		return MS::kFailure;

	// ── Read inputs ──────────────────────────────────────────────────

	MObject inputMeshObj = dataBlock.inputValue(attr_inputMesh).asMesh();
	if (inputMeshObj.isNull())
		return MS::kFailure;

	MArrayDataHandle colliderArrayHandle = dataBlock.inputArrayValue(attr_colliderMesh);
	const unsigned int numColliders = colliderArrayHandle.elementCount();
	if (numColliders == 0)
		return MS::kFailure;

	const float pushOffset = dataBlock.inputValue(attr_pushOffset).asFloat();
	const float envelope = dataBlock.inputValue(attr_envelope).asFloat();
	const int iterations = dataBlock.inputValue(attr_iterations).asInt();
	const bool useProxyNormal = dataBlock.inputValue(attr_useProxyNormal).asBool();

	const MVector colorVec = dataBlock.inputValue(attr_drawColor).asVector();
	const float drawOpacity = dataBlock.inputValue(attr_drawOpacity).asFloat();
	const bool showColliders = dataBlock.inputValue(attr_drawColliders).asBool();
	const bool showContacts = dataBlock.inputValue(attr_drawContacts).asBool();

	// ── Copy garment mesh for output ─────────────────────────────────

	MFnMeshData outputMeshData;
	MObject outputMeshObj = outputMeshData.create();

	MFnMesh inputMeshFn(inputMeshObj);
	MPointArray garmentPoints;
	inputMeshFn.getPoints(garmentPoints, MSpace::kWorld);

	MFloatVectorArray garmentNormals;
	inputMeshFn.getVertexNormals(false, garmentNormals, MSpace::kWorld);

	const int numVerts = garmentPoints.length();

	// Store rest positions for drawing
	MPointArray restPoints(garmentPoints);

	// ── Build collider MFnMesh list ──────────────────────────────────

	struct ColliderInfo
	{
		MFnMesh* meshFn;
		MObject meshObj;
	};

	vector<ColliderInfo> colliders;
	for (unsigned int c = 0; c < numColliders; c++)
	{
		colliderArrayHandle.jumpToElement(c);
		MObject colliderObj = colliderArrayHandle.inputValue().asMesh();
		if (!colliderObj.isNull())
		{
			ColliderInfo info;
			info.meshObj = colliderObj;
			info.meshFn = new MFnMesh(colliderObj);
			colliders.push_back(info);
		}
	}

	if (colliders.empty())
	{
		// Cleanup
		return MS::kFailure;
	}

	// ── Solve collision ──────────────────────────────────────────────
	// For each iteration, for each garment vertex, test against all
	// collider meshes. If the vertex is inside a collider, push it
	// out along the collider surface normal + offset.

	vector<int> contactVerts;

	for (int iter = 0; iter < iterations; iter++)
	{
		// We need thread-safe contact tracking only on last iteration
		vector<bool> contactFlags(numVerts, false);

		tbb::parallel_for(tbb::blocked_range<int>(0, numVerts), [&](tbb::blocked_range<int>& r)
		{
			for (int i = r.begin(); i != r.end(); i++)
			{
				MPoint srcPoint = garmentPoints[i];
				MVector bestPush(0, 0, 0);
				double bestPushLen = 0.0;
				bool pushed = false;

				for (const auto& collider : colliders)
				{
					// Find closest point on collider surface
					MPoint closestPoint;
					MVector closestNormal;

					MPoint closestPt;
					int faceId;

					// getClosestPoint returns the closest point on the mesh
					if (collider.meshFn->getClosestPoint(srcPoint, closestPt, MSpace::kWorld, &faceId) != MS::kSuccess)
						continue;

					// Get the face normal at the closest point
					MVector faceNormal;
					collider.meshFn->getPolygonNormal(faceId, faceNormal, MSpace::kWorld);

					// Vector from closest surface point to garment vertex
					MVector toVertex(
						srcPoint.x - closestPt.x,
						srcPoint.y - closestPt.y,
						srcPoint.z - closestPt.z
					);

					// Dot product: negative means vertex is inside the collider
					double dot = toVertex.x * faceNormal.x
					           + toVertex.y * faceNormal.y
					           + toVertex.z * faceNormal.z;

					if (dot < pushOffset)
					{
						// Vertex is inside or too close — push it out
						double pushDist = pushOffset - dot;

						MVector pushVec;
						if (useProxyNormal)
						{
							// Push along the collider's surface normal
							pushVec = faceNormal * pushDist;
						}
						else
						{
							// Push along the garment vertex's own normal
							MVector vn(garmentNormals[i].x, garmentNormals[i].y, garmentNormals[i].z);
							pushVec = vn * pushDist;
						}

						double pushLen = pushVec.length();
						if (pushLen > bestPushLen)
						{
							bestPush = pushVec;
							bestPushLen = pushLen;
							pushed = true;
						}
					}
				}

				if (pushed)
				{
					garmentPoints[i] = MPoint(
						srcPoint.x + bestPush.x * envelope,
						srcPoint.y + bestPush.y * envelope,
						srcPoint.z + bestPush.z * envelope
					);
					contactFlags[i] = true;
				}
			}
		});

		// Collect contact vertices (last iteration only, for drawing)
		if (iter == iterations - 1 && showContacts)
		{
			for (int i = 0; i < numVerts; i++)
			{
				if (contactFlags[i])
					contactVerts.push_back(i);
			}
		}
	}

	// ── Create output mesh ───────────────────────────────────────────

	// Copy input mesh topology to output
	MIntArray polyCounts, polyConnects;
	inputMeshFn.getVertices(polyCounts, polyConnects);

	MFnMesh outputMeshFn;
	outputMeshFn.create(numVerts, inputMeshFn.numPolygons(), garmentPoints, polyCounts, polyConnects, outputMeshObj);

	dataBlock.outputValue(attr_outputMesh).setMObject(outputMeshObj);
	dataBlock.setClean(attr_outputMesh);

	// ── Store draw data ──────────────────────────────────────────────

	drawData.garmentPoints = garmentPoints;
	drawData.restPoints = restPoints;
	drawData.contactVertexIndices.clear();
	drawData.contactVertexIndices = contactVerts;
	drawData.garmentColor = MColor(colorVec.x, colorVec.y, colorVec.z, drawOpacity);
	drawData.colliderColor = MColor(colorVec.x * 0.5, colorVec.y * 0.5, colorVec.z * 0.5, drawOpacity * 0.5);
	drawData.contactColor = MColor(1.0f, 0.2f, 0.1f, 1.0f);

	// Get garment triangles for drawing
	inputMeshFn.getTriangles(drawData.triangleCounts, drawData.triangleIndices);

	// Get collider drawing data
	drawData.colliderPointsList.clear();
	drawData.colliderTriCountsList.clear();
	drawData.colliderTriIndicesList.clear();

	if (showColliders)
	{
		for (const auto& collider : colliders)
		{
			MPointArray pts;
			collider.meshFn->getPoints(pts, MSpace::kWorld);
			drawData.colliderPointsList.push_back(pts);

			MIntArray triCounts, triIndices;
			collider.meshFn->getTriangles(triCounts, triIndices);
			drawData.colliderTriCountsList.push_back(triCounts);
			drawData.colliderTriIndicesList.push_back(triIndices);
		}
	}

	// ── Cleanup ──────────────────────────────────────────────────────

	for (auto& collider : colliders)
		delete collider.meshFn;

	return MS::kSuccess;
}

MStatus MeshCollider::initialize()
{
	MFnTypedAttribute tAttr;
	MFnNumericAttribute nAttr;

	// ── Input: garment mesh ──────────────────────────────────────────

	attr_inputMesh = tAttr.create("inputMesh", "inMesh", MFnData::kMesh);
	tAttr.setHidden(true);
	addAttribute(attr_inputMesh);

	// ── Input: collider meshes (array) ───────────────────────────────

	attr_colliderMesh = tAttr.create("colliderMesh", "colMesh", MFnData::kMesh);
	tAttr.setArray(true);
	tAttr.setHidden(true);
	addAttribute(attr_colliderMesh);

	// ── Input: solver settings ───────────────────────────────────────

	attr_pushOffset = nAttr.create("pushOffset", "pushOff", MFnNumericData::kFloat, 0.05);
	nAttr.setMin(0.0);
	nAttr.setMax(10.0);
	nAttr.setKeyable(true);
	addAttribute(attr_pushOffset);

	attr_envelope = nAttr.create("envelope", "env", MFnNumericData::kFloat, 1.0);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);
	nAttr.setKeyable(true);
	addAttribute(attr_envelope);

	attr_iterations = nAttr.create("iterations", "iter", MFnNumericData::kInt, 1);
	nAttr.setMin(1);
	nAttr.setMax(10);
	nAttr.setKeyable(true);
	addAttribute(attr_iterations);

	attr_useProxyNormal = nAttr.create("useProxyNormal", "usePN", MFnNumericData::kBoolean, true);
	nAttr.setKeyable(true);
	addAttribute(attr_useProxyNormal);

	// ── Input: draw settings ─────────────────────────────────────────

	attr_drawColor = nAttr.create("drawColor", "drawColor", MFnNumericData::k3Double);
	nAttr.setDefault(0.0, 0.3, 0.6);
	nAttr.setMin(0, 0, 0);
	nAttr.setMax(1, 1, 1);
	nAttr.setKeyable(true);
	addAttribute(attr_drawColor);

	attr_drawOpacity = nAttr.create("drawOpacity", "drawOpacity", MFnNumericData::kFloat, 0.3);
	nAttr.setMin(0);
	nAttr.setMax(1);
	nAttr.setKeyable(true);
	addAttribute(attr_drawOpacity);

	attr_drawColliders = nAttr.create("drawColliders", "drawCol", MFnNumericData::kBoolean, true);
	nAttr.setKeyable(true);
	addAttribute(attr_drawColliders);

	attr_drawContacts = nAttr.create("drawContacts", "drawCon", MFnNumericData::kBoolean, true);
	nAttr.setKeyable(true);
	addAttribute(attr_drawContacts);

	// ── Output: deformed garment mesh ────────────────────────────────

	attr_outputMesh = tAttr.create("outputMesh", "outMesh", MFnData::kMesh);
	tAttr.setHidden(true);
	addAttribute(attr_outputMesh);

	// ── Attribute affects ────────────────────────────────────────────

	attributeAffects(attr_inputMesh, attr_outputMesh);
	attributeAffects(attr_colliderMesh, attr_outputMesh);
	attributeAffects(attr_pushOffset, attr_outputMesh);
	attributeAffects(attr_envelope, attr_outputMesh);
	attributeAffects(attr_iterations, attr_outputMesh);
	attributeAffects(attr_useProxyNormal, attr_outputMesh);
	attributeAffects(attr_drawColor, attr_outputMesh);
	attributeAffects(attr_drawOpacity, attr_outputMesh);
	attributeAffects(attr_drawColliders, attr_outputMesh);
	attributeAffects(attr_drawContacts, attr_outputMesh);

	return MS::kSuccess;
}

// ── Viewport drawing ─────────────────────────────────────────────────

void MeshCollider::drawUI(MHWRender::MUIDrawManager& drawManager)
{
	// Draw collider meshes as semi-transparent overlays
	for (size_t c = 0; c < drawData.colliderPointsList.size(); c++)
	{
		const MPointArray& pts = drawData.colliderPointsList[c];
		const MIntArray& triIndices = drawData.colliderTriIndicesList[c];

		MFloatPointArray positions(triIndices.length());
		MColorArray colors(triIndices.length(), drawData.colliderColor);

		for (unsigned int i = 0; i < triIndices.length(); i++)
			positions[i] = pts[triIndices[i]];

		drawManager.mesh(MHWRender::MUIDrawManager::kTriangles, positions, NULL, &colors);

		// Wireframe
		drawManager.setColor(MColor(0, 0, 0, 0.3f));
		for (unsigned int i = 0; i < triIndices.length(); i += 3)
		{
			MPoint p0 = pts[triIndices[i]];
			MPoint p1 = pts[triIndices[i + 1]];
			MPoint p2 = pts[triIndices[i + 2]];
			drawManager.line(p0, p1);
			drawManager.line(p1, p2);
			drawManager.line(p2, p0);
		}
	}

	// Draw contact points
	if (!drawData.contactVertexIndices.empty())
	{
		drawManager.setColor(drawData.contactColor);
		drawManager.setPointSize(4);

		for (int idx : drawData.contactVertexIndices)
		{
			if (idx < (int)drawData.garmentPoints.length())
				drawManager.point(drawData.garmentPoints[idx]);
		}
	}
}

// ── Draw override ────────────────────────────────────────────────────

MUserData* MeshColliderDrawOverride::prepareForDraw(
	const MDagPath& objPath,
	const MDagPath& cameraPath,
	const MHWRender::MFrameContext& frameContext,
	MUserData* oldData)
{
	MStatus stat;
	MObject obj = objPath.node(&stat);
	if (stat != MS::kSuccess)
		return NULL;

	auto* data = dynamic_cast<MeshColliderUserData*>(oldData);
	if (!data)
		data = new MeshColliderUserData();

	MFnDependencyNode locatorFn(obj);
	data->meshCollider = dynamic_cast<MeshCollider*>(locatorFn.userNode());

	return data;
}

void MeshColliderDrawOverride::addUIDrawables(
	const MDagPath& objPath,
	MHWRender::MUIDrawManager& drawManager,
	const MHWRender::MFrameContext& frameContext,
	const MUserData* data)
{
	auto* meshColliderData = dynamic_cast<const MeshColliderUserData*>(data);

	if (meshColliderData && meshColliderData->meshCollider)
	{
		drawManager.beginDrawable();
		meshColliderData->meshCollider->drawUI(drawManager);
		drawManager.endDrawable();
	}
}
