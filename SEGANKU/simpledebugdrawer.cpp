#include "simpledebugdrawer.h"


SimpleDebugDrawer::SimpleDebugDrawer() : debugMode(0)
{
	// initialise debugMode with 0 -> No Debug Mode
}


SimpleDebugDrawer::~SimpleDebugDrawer()
{
}

void SimpleDebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& fromColor, const btVector3& toColor)
{

	printf("TEST TEST TEST");
	glBegin(GL_LINES);
	glColor3f(fromColor.getX(), fromColor.getY(), fromColor.getZ());
	glVertex3d(from.getX(), from.getY(), from.getZ());
	glColor3f(toColor.getX(), toColor.getY(), toColor.getZ());
	glVertex3d(to.getX(), to.getY(), to.getZ());
	glEnd();
}

void SimpleDebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
	drawLine(from, to, color, color);
}

void SimpleDebugDrawer::drawSphere(const btVector3& p, btScalar radius, const btVector3& color)
{
	glColor4f(color.getX(), color.getY(), color.getZ(), btScalar(1.0f));
	glPushMatrix();
	glTranslatef(p.getX(), p.getY(), p.getZ());

	int lats = 5;
	int longs = 5;

	int i, j;
	for (i = 0; i <= lats; i++) {
		btScalar lat0 = SIMD_PI * (-btScalar(0.5) + (btScalar)(i - 1) / lats);
		btScalar z0 = radius*sin(lat0);
		btScalar zr0 = radius*cos(lat0);

		btScalar lat1 = SIMD_PI * (-btScalar(0.5) + (btScalar)i / lats);
		btScalar z1 = radius*sin(lat1);
		btScalar zr1 = radius*cos(lat1);

		glBegin(GL_QUAD_STRIP);
		for (j = 0; j <= longs; j++) {
			btScalar lng = 2 * SIMD_PI * (btScalar)(j - 1) / longs;
			btScalar x = cos(lng);
			btScalar y = sin(lng);

			glNormal3f(x * zr0, y * zr0, z0);
			glVertex3f(x * zr0, y * zr0, z0);
			glNormal3f(x * zr1, y * zr1, z1);
			glVertex3f(x * zr1, y * zr1, z1);
		}
		glEnd();
	}

	glPopMatrix();
}



void SimpleDebugDrawer::drawTriangle(const btVector3& a, const btVector3& b, const btVector3& c, const btVector3& color, btScalar alpha)
{
	//	if (m_debugMode > 0)
	{
		const btVector3	n = btCross(b - a, c - a).normalized();
		glBegin(GL_TRIANGLES);
		glColor4f(color.getX(), color.getY(), color.getZ(), alpha);
		glNormal3d(n.getX(), n.getY(), n.getZ());
		glVertex3d(a.getX(), a.getY(), a.getZ());
		glVertex3d(b.getX(), b.getY(), b.getZ());
		glVertex3d(c.getX(), c.getY(), c.getZ());
		glEnd();
	}
}

void SimpleDebugDrawer::setDebugMode(int debugMode)
{
	debugMode = debugMode;
}

void SimpleDebugDrawer::draw3dText(const btVector3& location, const char* textString)
{
}

void SimpleDebugDrawer::reportErrorWarning(const char* warningString)
{
}

void SimpleDebugDrawer::drawContactPoint(const btVector3& pointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color)
{
}
