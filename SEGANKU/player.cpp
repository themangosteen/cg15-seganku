#include "player.h"

Player::CameraNavigationMode Player::cameraNavMode = FOLLOW_PLAYER;
double Player::scrollY = 0.0;

Player::Player(const glm::mat4 &matrix_, Camera *camera_, GLFWwindow *window_, const std::string &filePath)
    : Geometry(matrix_, filePath)
    , camera(camera_)
    , window(window_)
	, eatenCarrots(0)
	, fullStomach(false)
	, inBush(false)
	, currentFood(nullptr)
	, animDur(0)
{
	// set glfw callbacks
	glfwSetScrollCallback(window, onScroll);

	camera->setTransform(glm::translate(glm::mat4(1.0f), getLocation()+glm::vec3(0,2,6)));  //move camera back a bit
	lastCamTransform = camera->getMatrix();
	camDirection = glm::normalize(camera->getLocation() - getLocation());
	camUp = glm::vec3(0, 1, 0);
	camRight = glm::normalize(glm::cross(camUp, camDirection));

	playerShape = new btSphereShape(1);
	btTransform playerTransform;
	playerTransform.setIdentity();
	playerTransform.setOrigin(btVector3(getLocation().x, getLocation().y, getLocation().z));

	btScalar mass(1.0); btVector3 inertia(0, 0, 0);
	playerShape->calculateLocalInertia(mass, inertia);

	motionState = new btDefaultMotionState(playerTransform);
	btRigidBody::btRigidBodyConstructionInfo info(mass, motionState, playerShape, inertia);
	playerBody = new btRigidBody(info);
	playerBody->setActivationState(DISABLE_DEACTIVATION);
	playerBody->setCollisionFlags(playerBody->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
	playerBody->setUserPointer(this);

}

Player::~Player()
{
	delete camera; camera = nullptr;
	window = nullptr;
}

void Player::update(float timeDelta)
{
	if (eatenCarrots == NEEDED_FOOD) {
		fullStomach = true;
		return;
	}

	if (currentFood != nullptr) {
		if (animDur < 3.5f) {
			animDur += timeDelta;
			currentFood->setLocation(getLocation() + glm::vec3(0, 2, 0));
			currentFood->rotateY(glm::radians(10.0), SceneObject::RIGHT);
		}
		else {
			animDur = 0;
			currentFood->setLocation(glm::vec3(300, -300, 300));
			currentFood = nullptr;
		}
	}

	// note: camera navigation mode is toggled on tab key press, look for keyCallback
	handleNavModeChange();

	if (cameraNavMode == FOLLOW_PLAYER) {

		handleInput(window, timeDelta);
		glm::vec3 v = glm::normalize(getLocation() - camera->getLocation()) * 5.0f;
		viewMat = glm::lookAt(getLocation()-v, getLocation() + glm::vec3(0, 1, 0), camUp);
	}
	else {

		handleInputFreeCamera(window, timeDelta);
		viewMat = camera->getViewMatrix();
	}

	projMat = camera->getProjectionMatrix();


}

void Player::draw(Shader *shader, bool useFrustumCulling, Texture::FilterType filterType)
{
	Geometry::draw(shader, camera, useFrustumCulling, filterType);
}

void Player::handleInput(GLFWwindow *window, float timeDelta)
{

	// because we use bullet for motion, moveSpeed has to be quiet high for realistic feel
	float moveSpeed = 400.0f;
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
		moveSpeed = 800.0f;
	}

	glm::vec3 dirWorld = glm::normalize(glm::vec3(glm::column(getMatrix(), 2)));

	// player movement
	// note: we apply rotation before translation since we dont want the distance from the origin
	// to affect how we rotate
    if (glfwGetKey(window, 'W')) {
		playerBody->setLinearVelocity(btVector3(dirWorld.x, dirWorld.y, dirWorld.z) * timeDelta * moveSpeed);
		btTransform trans; 
		playerBody->getMotionState()->getWorldTransform(trans);//->getWorldTransform(trans);

		setLocation(glm::vec3(trans.getOrigin().getX(), trans.getOrigin().getY(), trans.getOrigin().getZ()));
    }
	else if (glfwGetKey(window, 'S')) {
		playerBody->setLinearVelocity(btVector3(-dirWorld.x, -dirWorld.y, -dirWorld.z) * timeDelta * moveSpeed);
		//btTransform trans = playerBody->getWorldTransform();//->getWorldTransform(trans);
		btTransform trans;
		playerBody->getMotionState()->getWorldTransform(trans);//->getWorldTransform(trans);

		setLocation(glm::vec3(trans.getOrigin().getX(), trans.getOrigin().getY(), trans.getOrigin().getZ()));
	}
	else {
		playerBody->setLinearVelocity(btVector3(0, 0, 0) * timeDelta * moveSpeed * 2);

		btTransform trans;
		playerBody->getMotionState()->getWorldTransform(trans);//->getWorldTransform(trans);

		setLocation(glm::vec3(trans.getOrigin().getX(), trans.getOrigin().getY(), trans.getOrigin().getZ()));
	}

	if (glfwGetKey(window, 'A')) {
		rotateY(timeDelta * glm::radians(200.f), SceneObject::RIGHT);
    }
	else if (glfwGetKey(window, 'D')) {
		rotateY(timeDelta * glm::radians(-200.f), SceneObject::RIGHT);
    }

	
	//// rotate camera based on mouse movement
	//// the mouse pointer is reset to (0, 0) every frame, and we just take the displacement of that frame
	const float mouseSensitivity = 0.01f;
	double mouseX, mouseY;
	glfwGetCursorPos(window, &mouseX, &mouseY);

	glm::vec3 camToTarget = camera->getLocation() - getLocation();
	glm::vec3 rightVec = glm::normalize(glm::cross(camToTarget, glm::vec3(0, 1, 0)));
	glm::mat4 rotateYaw = glm::rotate(glm::mat4(), -mouseSensitivity * (float)mouseX, glm::vec3(0, 1, 0));
	glm::mat4 rotatePitch = glm::rotate(glm::mat4(), mouseSensitivity * (float)mouseY, rightVec);

	camToTarget = glm::vec3(rotateYaw * glm::vec4(camToTarget, 0));
	camToTarget = glm::vec3(rotatePitch * glm::vec4(camToTarget, 0));
	camToTarget = camToTarget + getLocation();
	camera->setLocation(camToTarget);

	glfwSetCursorPos(window, 0, 0); // reset the mouse, so it doesn't leave the window


	//// handle camera zoom by changing the field of view depending on mouse scroll since last frame
	float zoomSensitivity = -0.1f;
	float fieldOfView = camera->getFieldOfView() + zoomSensitivity * (float)scrollY;
	if (fieldOfView < glm::radians(ZOOM_MIN)) fieldOfView = glm::radians(ZOOM_MIN);
	if (fieldOfView > glm::radians(ZOOM_MAX)) fieldOfView = glm::radians(ZOOM_MAX);
	camera->setFieldOfView(fieldOfView);
	scrollY = 0.0;	
}

void Player::handleInputFreeCamera(GLFWwindow *window, float timeDelta)
{

	float moveSpeed = 10.0f;
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
		moveSpeed = 50.0f;
	}
	
	//////////////////////////
	/// CAMERA MOVEMENT
	//////////////////////////

	// camera movement
	// note: we apply rotation before translation since we dont want the distance from the origin
	// to affect how we rotate
    if (glfwGetKey(window, 'W')) {
		camera->translate(camera->getMatrix()[2].xyz() * -timeDelta * moveSpeed, SceneObject::LEFT);
    }
	else if (glfwGetKey(window, 'S')) {
		camera->translate(camera->getMatrix()[2].xyz() * timeDelta * moveSpeed, SceneObject::LEFT);
	}

	if (glfwGetKey(window, 'A')) {
		camera->translate(camera->getMatrix()[0].xyz() * -timeDelta * moveSpeed, SceneObject::LEFT);
    }
	else if (glfwGetKey(window, 'D')) {
		camera->translate(camera->getMatrix()[0].xyz() * timeDelta * moveSpeed, SceneObject::LEFT);
    }

	if (glfwGetKey(window, 'Q')) {
	    camera->translate(glm::vec3(0,1,0) * timeDelta * moveSpeed, SceneObject::LEFT);
	}
	else if (glfwGetKey(window, 'E')) {
	    camera->translate(glm::vec3(0,1,0) * -timeDelta * moveSpeed, SceneObject::LEFT);
	}

	// rotate camera based on mouse movement
	// the mouse pointer is reset to (0, 0) every frame, and we just take the displacement of that frame
	const float mouseSensitivity = 0.01f;
	double mouseX, mouseY;
	glfwGetCursorPos(window, &mouseX, &mouseY);
	camera->rotateX(-mouseSensitivity * (float)mouseY, SceneObject::RIGHT); // rotate around local x axis (tilt up/down)
	glm::vec3 location = camera->getLocation();
	camera->translate(-location, SceneObject::LEFT);
	camera->rotateY(-mouseSensitivity * (float)mouseX, SceneObject::LEFT); // rotate around global y at local position
	camera->translate(location, SceneObject::LEFT);
	glfwSetCursorPos(window, 0, 0); // reset the mouse, so it doesn't leave the window

	// handle camera zoom by changing the field of view depending on mouse scroll since last frame
	float zoomSensitivity = -0.1f;
	float fieldOfView = camera->getFieldOfView() + zoomSensitivity * (float)scrollY;
	if (fieldOfView < glm::radians(ZOOM_MIN)) fieldOfView = glm::radians(ZOOM_MIN);
	if (fieldOfView > glm::radians(ZOOM_MAX)) fieldOfView = glm::radians(ZOOM_MAX);
	camera->setFieldOfView(fieldOfView);
	scrollY = 0.0;

}

void Player::onScroll(GLFWwindow *window, double deltaX, double deltaY)
{
	scrollY += deltaY;
}

void Player::toggleNavMode()
{
	if (cameraNavMode == FOLLOW_PLAYER) {
		cameraNavMode = FREE_FLY;
	}
	else if (cameraNavMode == FREE_FLY) {
		cameraNavMode = FOLLOW_PLAYER;
	}
}

void Player::handleNavModeChange()
{
	if (cameraNavMode == lastNavMode) {
		return;
	}

	glm::mat4 temp = camera->getMatrix();
	camera->setTransform(lastCamTransform);
	lastCamTransform = temp;

	lastNavMode = cameraNavMode;
}

void Player::eatCarrot(Geometry *carrot)
{
	if (!(currentFood != nullptr)) {
		++eatenCarrots;
		currentFood = carrot;
	}
}

bool Player::isFull()
{
	return fullStomach;
}

bool Player::isInBush()
{
	return inBush;
}

void Player::setInBush(bool inB)
{
	inBush = inB;
}

int Player::getFoodEaten()
{
	return eatenCarrots;
}

glm::mat4 Player::getViewMat()
{
	return viewMat;
}

glm::mat4 Player::getProjMat()
{
	return projMat;
}

btRigidBody *Player::getRigidBody()
{
	return playerBody;
}
