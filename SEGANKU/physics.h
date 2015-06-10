#ifndef PHYSICS_H
#define PHYSICS_H

#include <btBulletDynamicsCommon.h>
#include "simpledebugdrawer.h"

class Physics
{
public:
	Physics();
	~Physics();

	void init();
	void cleanUp();

	void stepSimulation(float deltaT);
	void debugDrawWorld(bool draw);

	SimpleDebugDrawer *getPhysicsDebugDrawer();
	btDiscreteDynamicsWorld *getDynamicsWorld();

private:
	bool drawDebug;
	btDefaultCollisionConfiguration *collisionConfiguration;
	btCollisionDispatcher *dispatcher;
	btBroadphaseInterface * overlappingPairCache;
	btSequentialImpulseConstraintSolver *solver;
	
	btDiscreteDynamicsWorld *dynamicsWorld;

	btAlignedObjectArray<btCollisionShape*> collisionShapes;
	
	SimpleDebugDrawer *debugDrawerPhysics;
};

#endif// PHYSICS_H

