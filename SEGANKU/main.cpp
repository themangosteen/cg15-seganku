#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <btBulletDynamicsCommon.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <random>
#include <ctime>

#include "shader.h"
#include "sceneobject.h"
#include "camera.h"
#include "player.h"
#include "eagle.h"
#include "light.h"
#include "textrenderer.h"
#include "effects/ssaopostprocessor.h"
#include "effects/particlesystem.h"
#include "poissondisksampler.h"
#include "simpledebugdrawer.h"
#include "physics.h"

void init(GLFWwindow *window);
void initWorldBounds(float &miX, float &maX, float &miY, float &maY);
float terrainGetYCoord(glm::vec2 pos2D, float maxDistanceXY, float offsetY);
void initSM();
void initVSM();
void initPCFSM();
void initVSMBlur();
void shadowFirstPass(glm::mat4 &lightViewPro);
void vsmBlurPass();
void debugShadowPass();
void ssaoFirstPass();
void finalDrawPass();
void initPhysicsObjects();
void update(float timeDelta);
void setActiveShader(Shader *shader);
void drawScene();
void drawText(double deltaT, int windowWidth, int windowHeight);
void cleanup();
void newGame();

GLFWwindow *window;
int windowWidth, windowHeight;
bool running			       = true;
bool paused				       = false;
bool debugInfoEnabled          = true;
bool wireframeEnabled          = false;
bool ssaoEnabled		       = true;
bool ssaoBlurEnabled	       = true;
bool shadowsEnabled		       = true;
bool vsmShadowsEnabled		   = false;
bool renderShadowMap	       = false;
bool frustumCullingEnabled     = false;
bool useAlpha				   = false;

Texture::FilterType filterType = Texture::LINEAR_MIPMAP_LINEAR;

Shader *textureShader, *depthMapShader, *vsmDepthMapShader, *debugDepthShader, *blurVSMDepthShader;
Shader *activeShader;
TextRenderer *textRenderer;
ParticleSystem *particleSystem;
SSAOPostprocessor *ssaoPostprocessor;

Player *player; glm::mat4 playerInitTransform(glm::scale(glm::mat4(1.0f), glm::vec3(0.5, 0.5, 0.5)));
Eagle *eagle; glm::mat4 eagleInitTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0, 30, -45)));

Geometry *terrain, *cave;

Camera *camera;
Light *sun;
const glm::vec3 LIGHT_START(glm::vec3(-100, 150, 0));
const glm::vec3 LIGHT_END(glm::vec3(20, 150, 0));

std::vector<std::shared_ptr<Geometry>> carrots;
std::vector<std::shared_ptr<Geometry>> trees;
std::vector<std::shared_ptr<Geometry>> shrubs;
const float timeToStarvation = 60;

// Shadow Map FBO and depth texture
GLuint depthMapFBO, vsmDepthMapFBO;
GLuint depthMap, vsmDepthMap;
GLuint pingpongFBO;
GLuint pingpongColorMap;

const int SM_WIDTH = 1024, SM_HEIGHT = 1024;
const GLfloat NEAR_PLANE = 75.f, FAR_PLANE = 250.f;

void frameBufferResize(GLFWwindow *window, int width, int height);
void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);

// PHYSICS

Physics *physics;

// ONLY FOR SEBAS DEBUGGING
GLuint quadVAO = 0;
GLuint quadVBO;
void RenderQuad()
{
	if (quadVAO == 0)
	{
		GLfloat quadVertices[] = {
			// Positions         // Texture Coords
			-1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
		};
		// Setup plane VAO
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}
// SEBAS DEBUGGING END


int main(int argc, char **argv)
{
	// HANDLE COMMAND LINE PARAMETERS

	btCollisionShape *shape = new btBoxShape(btVector3(btScalar(50.), btScalar(50.), btScalar(50.)));

    windowWidth = 800;
    windowHeight = 600;
	int refresh_rate = 60;
    bool fullscreen = 0;

	if (argc == 1) {
		// no parameters specified, continue with default values

	} else if (argc != 4 || (std::stringstream(argv[1]) >> windowWidth).fail() || (std::stringstream(argv[2]) >> windowHeight).fail() || (std::stringstream(argv[3]) >> fullscreen).fail()) {
		// if parameters are specified, must conform to given format

		std::cout << "USAGE: <resolution width> <resolution height> <fullscreen? 0/1>\n";
		exit(EXIT_FAILURE);
	}

	// INIT WINDOW AND OPENGL CONTEXT

	if (!glfwInit()) {
		exit(EXIT_FAILURE);
	}

	glfwWindowHint(GLFW_REFRESH_RATE, refresh_rate);
	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWmonitor *monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode *videoMode = glfwGetVideoMode(monitor);

	window = nullptr;
	window = glfwCreateWindow(windowWidth, windowHeight, "SEGANKU", (fullscreen ? monitor : NULL), NULL);
	if (!window)
	{
		std::cerr << "ERROR: Failed to open GLFW window.\n";
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	// center window on screen
	glfwSetWindowPos(window, videoMode->width/2 - windowWidth/2, videoMode->height/2 - windowHeight/2);

	glfwMakeContextCurrent(window);

	// capture mouse pointer and hide it
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPos(window, 0, 0);

	// Clear Color and set Viewport
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glViewport(0, 0, windowWidth, windowHeight);

	// print OpenGL version
	std::cout << "OpenGL " << glGetString(GL_VERSION) << std::endl;
	std::cout << "GLSL " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (err != GLEW_OK) {
		std::cerr << glewGetErrorString(err);
	}

	// set callbacks
	glfwSetFramebufferSizeCallback(window, frameBufferResize);
	glfwSetKeyCallback(window, keyCallback);

	// all initializations happen here
	init(window);

	//////////////////////////
	/// MAIN LOOP
	//////////////////////////

	double time = 0.0;
	double lastTime = 0.0;
	double deltaT = 0.0;

	while (running && !glfwWindowShouldClose(window)) {

		if (glfwGetTime() < lastTime) {
			lastTime = 0;
		}
		time = glfwGetTime(); // seconds
		deltaT = time - lastTime;
		lastTime = time;

		// glUseProgram calls are rather expensive state changes, so try to keep to a minimum
		// if more shaders are used for different objects, restructuring of these calls will be necessary
		// since a lot of other calls depend on the currently bound shader
		setActiveShader(textureShader);

		//////////////////////////
		/// UPDATE
		//////////////////////////
		if (!paused) {

			physics->stepSimulation(deltaT);
			update(deltaT);

			// pause on starvation or if player eaten by eagle
			if (eagle->isTargetEaten() || glfwGetTime() > timeToStarvation-1) {
				player->rotateZ(3.14159/2, SceneObject::RIGHT);
				player->translate(glm::vec3(0, 0.3, 0), SceneObject::LEFT);
				paused = true;
			}
		}


		//////////////////////////
		/// DRAW
		//////////////////////////

		//// SHADOW MAP PASS
		// calculate lights projection and view Matrix
		glm::mat4 lightVP;

		if (shadowsEnabled) {
			shadowFirstPass(lightVP);
		}

		// Prepare lighting shader and set matrices
		setActiveShader(textureShader);
		glUniformMatrix4fv(glGetUniformLocation(activeShader->programHandle, "viewMat"), 1, GL_FALSE, glm::value_ptr(player->getViewMat()));
		glUniformMatrix4fv(glGetUniformLocation(activeShader->programHandle, "lightVP"), 1, GL_FALSE, glm::value_ptr(lightVP));

		//if (vsmShadowsEnabled) {
			glUniform1i(glGetUniformLocation(activeShader->programHandle, "shadowMap"), 1);
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_2D, vsmDepthMap);
		/*}
		else {
			glUniform1i(glGetUniformLocation(activeShader->programHandle, "shadowMap"), 1);
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_2D, depthMap);
		}*/

		//// SSAO PrePass (if enabled)
		ssaoFirstPass();

		//// FINAL PASS
		//// draw with shadow mapping and ssao
		finalDrawPass();

		// draw shadow map for debugging (if enabled)
		debugShadowPass();

		particleSystem->draw(player->getViewMat(), player->getProjMat(), glm::vec3(1, 0.55, 0.5));

		drawText(deltaT, windowWidth, windowHeight);

		// end the current frame (swaps the front and back buffers)
		glfwSwapBuffers(window);


		//////////////////////////
		/// ERRORS AND EVENTS
		//////////////////////////
		GLenum glErr = glGetError();
		if (glErr != GL_NO_ERROR) {
			// handle errors
			std::cerr << "ERROR: OpenGL Error " << glErr << std::endl;
		}

		glfwPollEvents();

		if (running) {
			running = !glfwGetKey(window, GLFW_KEY_ESCAPE);
		}
	}

	// release resources
	cleanup();

	glfwTerminate();
	exit(EXIT_SUCCESS); // for system independent success code
	return 0; // to silence compiler warnings
}


void init(GLFWwindow *window)
{
	// enable z buffer test
	glEnable(GL_DEPTH_TEST);

	// INIT SHADOW MAPPING (FBO, Texture, Shader)
	initSM();

	int width, height;
	glfwGetWindowSize(window, &width, &height);

	paused = false;

	// INIT TEXT RENDERER
	textRenderer = new TextRenderer("../data/fonts/cliff.ttf", width, height);

	// INIT PARTICLE SYSTEM
	particleSystem = new ParticleSystem(glm::mat4(1.0f), "../data/models/skunk/smoke.png", 30, 100.f, 15.f, -0.05f);

	// INIT SSAO POST PROCESSOR
    ssaoPostprocessor = new SSAOPostprocessor(width, height, 32);

	// INIT SHADERS
	textureShader = new Shader("../SEGANKU/shaders/textured_blinnphong.vert", "../SEGANKU/shaders/textured_blinnphong.frag");
	setActiveShader(textureShader); // non-trivial cost
	// note that the following initializations are intended to be used with a shader of the structure like textureShader
	// so dont activate any shader of different structure before those initializations are done


	// INIT WORLD + OBJECTS

	sun = new Light(glm::translate(glm::mat4(1.0f), LIGHT_START), LIGHT_END, glm::vec3(1.f, 0.89f, 0.6f), glm::vec3(0.87f, 0.53f, 0.f), timeToStarvation);

	terrain = new Geometry(glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1)), "../data/models/world/terrain.dae");
	float minX, maxX, minZ, maxZ;
	initWorldBounds(minX, maxX, minZ, maxZ);

	// cave
	glm::vec2 cavePos2D(0, 0);
	cave = new Geometry(glm::mat4(1.0f), "../data/models/cave/cave.dae");
	cave->setLocation(glm::vec3(cavePos2D.x, terrainGetYCoord(cavePos2D, 5.0f, -0.4f), cavePos2D.y));

	std::default_random_engine randGen(time(nullptr));
	std::uniform_real_distribution<float> randDistribution(0.0f, 1.0f);
	auto rand = std::bind(randDistribution, std::ref(randGen));

	float y = 0.0f;
	// procedurally placed carrots
	std::vector<glm::vec2> positions = PoissonDiskSampler::generatePoissonSample(80, 0.4f); // positions in range [0, 1]
	for (glm::vec2 p : positions) {
		p = (p - glm::vec2(0.5, 0.5)) * glm::max(maxX, maxZ)*1.8f;
		if (p.x > minX && p.x < maxX && p.y > minZ && p.y < maxZ && glm::distance(p, cavePos2D) > 10) {
			y = terrainGetYCoord(p, 0.5f, -0.2f);
			carrots.push_back(std::make_shared<Geometry>(glm::translate(glm::rotate(glm::mat4(1.0f), 0.0f, glm::vec3(0, 1, 0)), glm::vec3(p.x, y, p.y)), "../data/models/world/carrot.dae"));
		}
	}

	// procedurally placed trees
	positions = PoissonDiskSampler::generatePoissonSample(60, 0.4f); // positions in range [0, 1]
	for (glm::vec2 p : positions) {
		p = (p - glm::vec2(0.5, 0.5)) * glm::max(maxX, maxZ)*1.8f;
		if (p.x > minX && p.x < maxX && p.y > minZ && p.y < maxZ && glm::distance(p, cavePos2D) > 7) {
			y = terrainGetYCoord(p, 1.5f, -1.0f);
			trees.push_back(std::make_shared<Geometry>(glm::translate(glm::rotate(glm::scale(glm::mat4(1.0f), glm::vec3(0.75 + rand() / 2)), rand() * 2 * glm::pi<float>(), glm::vec3(0, 1, 0)), glm::vec3(p.x, y, p.y)), "../data/models/world/tree.dae")); //rand() * 2 * glm::pi<float>()
		}
	}

	// procedurally placed shrubs
	positions = PoissonDiskSampler::generatePoissonSample(40, 0.6f); // positions in range [0, 1]
	for (unsigned int i = 0; i < positions.size(); ++i) {
		glm::vec2 p = positions[i];
		p = (p - glm::vec2(0.5, 0.5)) * glm::max(maxX, maxZ)*1.8f;
		if (p.x > minX && p.x < maxX && p.y > minZ && p.y < maxZ && glm::distance(p, cavePos2D) > 10) {
			y = terrainGetYCoord(p, 1.0f, -0.4f);
			if (i % 2 == 0) {
				shrubs.push_back(std::make_shared<Geometry>(glm::translate(glm::rotate(glm::scale(glm::mat4(1.0f), glm::vec3(1 + rand() / 2)), 0.0f, glm::vec3(0, 1, 0)), glm::vec3(p.x, y, p.y)), "../data/models/world/shrub1.dae"));
			}
			else {
				shrubs.push_back(std::make_shared<Geometry>(glm::translate(glm::rotate(glm::scale(glm::mat4(1.0f), glm::vec3(1 + rand() / 2)), rand() * 2 * glm::pi<float>(), glm::vec3(0, 1, 0)), glm::vec3(p.x, y, p.y)), "../data/models/world/shrub2.dae"));
			}
		}
	}


	// INIT PLAYER + CAMERA
	camera = new Camera(glm::mat4(1.0f), glm::radians(80.0f), width/(float)height, 0.2f, 200.0f); // mat, fov, aspect, znear, zfar
	player = new Player(playerInitTransform, camera, window, "../data/models/skunk/skunk.dae");


	// INIT EAGLE
	eagle = new Eagle(eagleInitTransform, "../data/models/eagle/eagle.dae");


	// INIT PHYSICS OBJECTS (add objects to dynamic World)
	initPhysicsObjects();

	glfwSetTime(0);
}


void initSM()
{
	// SM Shaders
	depthMapShader = new Shader("../SEGANKU/shaders/depth_shader.vert", "../SEGANKU/shaders/depth_shader.frag");
	debugDepthShader = new Shader("../SEGANKU/shaders/quad_debug.vert", "../SEGANKU/shaders/quad_debug.frag");
	vsmDepthMapShader = new Shader("../SEGANKU/shaders/depth_shader_vsm.vert", "../SEGANKU/shaders/depth_shader_vsm.frag");
	blurVSMDepthShader = new Shader("../SEGANKU/shaders/blur_vsm.vert", "../SEGANKU/shaders/blur_vsm.frag");

	initVSM();
	//initPCFSM();
	initVSMBlur();
}


void initPCFSM()
{
	// INIT SHADOW MAPPING (Framebuffer + ShadowMap + Shaders)
	glGenFramebuffers(1, &depthMapFBO);
	glGenTextures(1, &depthMap);

	// ShadowMap
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SM_WIDTH, SM_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	// SM Framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void initVSM()
{
	// INIT SHADOW MAPPING (Framebuffer + ShadowMap + Shaders)
	glGenFramebuffers(1, &vsmDepthMapFBO);

	// ShadowMomentsMap
	glGenTextures(1, &vsmDepthMap);
	glBindTexture(GL_TEXTURE_2D, vsmDepthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, SM_WIDTH, SM_HEIGHT, 0, GL_RGBA, GL_FLOAT, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glGenerateMipmap(GL_TEXTURE_2D);

	// SM Framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, vsmDepthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vsmDepthMap, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void initVSMBlur()
{
	glGenFramebuffers(1, &pingpongFBO);
	glGenTextures(1, &pingpongColorMap);

	glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO);
	glBindTexture(GL_TEXTURE_2D, pingpongColorMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, SM_WIDTH, SM_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorMap, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void initPhysicsObjects()
{
	physics = new Physics(player);
	physics->init();

	physics->getDynamicsWorld()->addRigidBody(player->getRigidBody());

	for (std::vector<std::shared_ptr<Geometry>>::iterator it = trees.begin(); it != trees.end(); ++it) {
		physics->addTreeCylinderToPhysics(it->get(), btScalar(0.6));
	}

	for (std::vector<std::shared_ptr<Geometry>>::iterator it = carrots.begin(); it != carrots.end(); ++it) {
		physics->addFoodSphereToPhysics(it->get(), btScalar(0.3));
	}

	for (std::vector<std::shared_ptr<Geometry>>::iterator it = shrubs.begin(); it != shrubs.end(); ++it) {
		physics->addBushSphereToPhysics(it->get(), btScalar(2));
	}

	physics->addTerrainShapeToPhysics(terrain);
	physics->setupCaveObjects(cave);

}


void update(float timeDelta)
{
	player->update(timeDelta);
	player->setIsInCave(glm::distance(player->getLocation(), cave->getLocation()) < 2.0f);

	eagle->update(timeDelta, player->getLocation() + glm::vec3(0, 2, 0), player->isInBush() || player->isInCave(), player->isDefenseActive());

	particleSystem->update(timeDelta, player->getViewMat());

	sun->update(timeDelta);

	// SET POSITION AND COLOR IN SHADERS

	GLint lightPosLocation = glGetUniformLocation(activeShader->programHandle, "light.position");
	GLint lightAmbientLocation = glGetUniformLocation(activeShader->programHandle, "light.ambient");
	GLint lightDiffuseLocation = glGetUniformLocation(activeShader->programHandle, "light.diffuse");
	GLint lightSpecularLocation = glGetUniformLocation(activeShader->programHandle, "light.specular");

	GLint materialSpecularLocation = glGetUniformLocation(activeShader->programHandle, "material.specular");
	glUniform3f(materialSpecularLocation, 0.2f, 0.2f, 0.2f);

	glUniform3f(lightPosLocation, sun->getLocation().x, sun->getLocation().y, sun->getLocation().z);
	glUniform3f(lightAmbientLocation, sun->getColor().x * 0.3f, sun->getColor().y * 0.3f, sun->getColor().z * 0.3f);
	glUniform3f(lightDiffuseLocation, sun->getColor().x, sun->getColor().y, sun->getColor().z);
	glUniform3f(lightSpecularLocation, sun->getColor().x * 0.8f, sun->getColor().y * 0.8f, sun->getColor().z * 0.8f);

}


void drawScene()
{
	if (wireframeEnabled) glPolygonMode( GL_FRONT_AND_BACK, GL_LINE ); // enable wireframe

	if (useAlpha) {
		glUniform1f(glGetUniformLocation(activeShader->programHandle, "useAlpha"), true);
	}
	else {
		glUniform1f(glGetUniformLocation(activeShader->programHandle, "useAlpha"), false);
	}

	// pass viewProjection matrix to shader
	GLint viewProjMatLocation = glGetUniformLocation(activeShader->programHandle, "viewProjMat"); // get uniform location in shader
	glUniformMatrix4fv(viewProjMatLocation, 1, GL_FALSE, glm::value_ptr(player->getProjMat() * player->getViewMat())); // shader location, count, transpose?, value pointer

	// pass camera position to shader
	GLint cameraPosLocation = glGetUniformLocation(activeShader->programHandle, "cameraPos");
	glUniform3f(cameraPosLocation, camera->getLocation().x, camera->getLocation().y, camera->getLocation().z);

	physics->debugDrawWorld(true);

	// DRAW GEOMETRY

	Geometry::drawnSurfaceCount = 0;

	glUniform1f(glGetUniformLocation(activeShader->programHandle, "material.shininess"), 64.f);
	terrain->draw(activeShader, camera, false, filterType, player->getViewMat());

	glUniform1f(glGetUniformLocation(activeShader->programHandle, "material.shininess"), 2.f);
	for (std::shared_ptr<Geometry> carr : carrots) {
		carr->draw(activeShader, camera, frustumCullingEnabled, filterType, player->getViewMat());
	}

	glUniform1f(glGetUniformLocation(activeShader->programHandle, "material.shininess"), 16.f);
	player->draw(activeShader, frustumCullingEnabled, filterType, player->getViewMat());

	for (std::shared_ptr<Geometry> shrub : shrubs) {
		shrub->draw(activeShader, camera, frustumCullingEnabled, filterType, player->getViewMat());
	}

	cave->draw(activeShader, camera, false, filterType, player->getViewMat());

	for (std::shared_ptr<Geometry> tree : trees) {
		tree->draw(activeShader, camera, frustumCullingEnabled, filterType, player->getViewMat());
	}

	glUniform1f(glGetUniformLocation(activeShader->programHandle, "material.shininess"), 32.f);
	eagle->draw(activeShader, camera, frustumCullingEnabled, filterType, player->getViewMat());

	if (wireframeEnabled) glPolygonMode( GL_FRONT_AND_BACK, GL_FILL ); // disable wireframe

}


void drawText(double deltaT, int windowWidth, int windowHeight)
{
	glDisable(GL_DEPTH_TEST);

	if (debugInfoEnabled) {

		int startY = 400;
		int deltaY = 20;
		float fontSize = 0.35f;
		textRenderer->renderText("drawn surface count: " + std::to_string(Geometry::drawnSurfaceCount), 25, startY+2*deltaY, fontSize, glm::vec3(1));
		textRenderer->renderText("delta time: " + std::to_string(int(deltaT*1000 + 0.5)) + " ms", 25, startY+3*deltaY, fontSize, glm::vec3(1));
		textRenderer->renderText("fps: " + std::to_string(int(1/deltaT + 0.5)), 25, startY+4*deltaY, fontSize, glm::vec3(1));

		if (!paused) {
			textRenderer->renderText("time until starvation: " + std::to_string(int(timeToStarvation - glfwGetTime())), 25.0f, startY+6*deltaY, fontSize, glm::vec3(1));
			textRenderer->renderText("player hidden: " + std::to_string(player->isInBush()) /* + ", defense active: " + std::to_string(player->isDefenseActive()) */, 25.0f, startY+7*deltaY, fontSize, glm::vec3(1));
			std::string eagleStateStrings[3] = {"CIRCLING", "ATTACKING", "RETREATING"};
			textRenderer->renderText("eagle state: " + eagleStateStrings[eagle->getState()] /* + ", in reach: " + std::to_string(eagle->isInTargetDefenseReach()) */, 25.0f, startY+8*deltaY, fontSize, glm::vec3(1));
		}
	}

	if (paused) {
		if (eagle->isTargetEaten()) {
			textRenderer->renderText("YOU GOT EATEN =(", 25.0f, 150.0f, 0.7f, glm::vec3(1, 0.35f, 0.7f));
		}
		else if (glfwGetTime() > timeToStarvation - 1) {
			textRenderer->renderText("YOU STARVED =(", 25.0f, 150.0f, 0.7f, glm::vec3(1, 0.35f, 0.5f));
		}
	}

	if (player->isFull() && player->isInCave()) {
		textRenderer->renderText("YOU MADE IT!!!", 25.0f, 150.0f, 0.7f, glm::vec3(1, 0.45f, 0.7f));
		paused = true;
	}

	std::string carrotText = "carrots: " + std::to_string(player->getFoodCount()) + " / " + std::to_string(player->getNeededFood());
	textRenderer->renderText(carrotText, 25.0f, 30.0f, 0.7f, glm::vec3(1, 0.7f, 0.0f));

	if (player->isEating()) {
		textRenderer->renderText(player->getFoodReaction(), 300.0f, 300.0f, 0.4f, glm::vec3(0.5f, 0.7f, 0.5f));
	}

	glEnable(GL_DEPTH_TEST);
}


void shadowFirstPass(glm::mat4 &lightViewPro)
{
	// Calculate Light View-Projection Matrix
	glm::mat4 lightProjection = glm::ortho(-100.0f, 100.0f, -100.0f, 100.0f, NEAR_PLANE, FAR_PLANE);
	//glm::mat4 lightProjection = glm::perspective(100.f, (GLfloat) SM_WIDTH / (GLfloat) SM_HEIGHT, nearPlane, farPlane);
	glm::mat4 lightView = glm::lookAt(sun->getLocation(), glm::vec3(0.f), glm::vec3(0, 1, 0));
	lightViewPro = lightProjection * lightView;

	// set viewport and bind framebuffer
	glViewport(0, 0, SM_WIDTH, SM_HEIGHT);

	//if (vsmShadowsEnabled) {
		glBindFramebuffer(GL_FRAMEBUFFER, vsmDepthMapFBO);
		setActiveShader(vsmDepthMapShader);
		glUniformMatrix4fv(glGetUniformLocation(activeShader->programHandle, "lightVP"), 1, GL_FALSE, glm::value_ptr(lightViewPro));
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		drawScene();
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		if (vsmShadowsEnabled) {
			vsmBlurPass();
		}
	/*}
	else {
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		setActiveShader(depthMapShader);
		glClear(GL_DEPTH_BUFFER_BIT);
		glUniformMatrix4fv(glGetUniformLocation(activeShader->programHandle, "lightVP"), 1, GL_FALSE, glm::value_ptr(lightViewPro));
		drawScene();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	*/
	// bind default FB and reset viewport
	glViewport(0, 0, windowWidth, windowHeight);
}


void vsmBlurPass()
{
	GLboolean horizontal = true;

	glViewport(0, 0, SM_WIDTH, SM_HEIGHT);
	setActiveShader(blurVSMDepthShader);

	glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO);
	glUniform1i(glGetUniformLocation(activeShader->programHandle, "horizontal"), horizontal);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, vsmDepthMap);
	RenderQuad();
	horizontal = !horizontal;

	glBindFramebuffer(GL_FRAMEBUFFER, vsmDepthMapFBO);
	glUniform1i(glGetUniformLocation(activeShader->programHandle, "horizontal"), horizontal);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, pingpongColorMap);
	RenderQuad();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void ssaoFirstPass()
{
	if (ssaoEnabled) {
		//// SSAO PREPASS
		//// draw ssao input data (screen colors and view space positions) to framebuffer textures
		ssaoPostprocessor->bindScreenDataFramebuffer();
		glUniform1i(glGetUniformLocation(activeShader->programHandle, "useShadows"), 0);
		glUniform1i(glGetUniformLocation(activeShader->programHandle, "useSSAO"), 0);
		glUniform1i(glGetUniformLocation(activeShader->programHandle, "useVSM"), 0);
		glClearColor(sun->getColor().x, sun->getColor().y, sun->getColor().z, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		drawScene();

		//// SSAO PASS
		//// draw ssao output data to framebuffer texture
		ssaoPostprocessor->calulateSSAOValues(player->getProjMat());
		setActiveShader(textureShader);

		//// SSAO BLUR PASS
		if (ssaoBlurEnabled) {
			ssaoPostprocessor->blurSSAOResultTexture();
			//ssaoPostprocessor->blurSSAOResultTexture();
			setActiveShader(textureShader);
		}
	}
}


void finalDrawPass()
{
	glClearColor(sun->getColor().x, sun->getColor().y, sun->getColor().z, 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUniform1i(glGetUniformLocation(activeShader->programHandle, "useShadows"), shadowsEnabled);
	glUniform1i(glGetUniformLocation(activeShader->programHandle, "useSSAO"), ssaoEnabled);
	glUniform1i(glGetUniformLocation(activeShader->programHandle, "useVSM"), vsmShadowsEnabled);

	ssaoPostprocessor->bindSSAOResultTexture(glGetUniformLocation(activeShader->programHandle, "ssaoTexture"), 2);

	drawScene();
}


void debugShadowPass()
{
	if (renderShadowMap) {

		setActiveShader(debugDepthShader);

		glUniform1f(glGetUniformLocation(debugDepthShader->programHandle, "near_plane"), NEAR_PLANE);
		glUniform1f(glGetUniformLocation(debugDepthShader->programHandle, "far_plane"), FAR_PLANE);
		glActiveTexture(GL_TEXTURE0);

		glBindTexture(GL_TEXTURE_2D, vsmDepthMap);

		RenderQuad();
	}
}


void newGame()
{
	delete sun;

	glfwSetTime(0);

	int width, height;
	glfwGetWindowSize(window, &width, &height);

	sun = new Light(glm::translate(glm::mat4(1.0f), LIGHT_START), LIGHT_END, glm::vec3(1.f, 0.89f, 0.6f), glm::vec3(0.87f, 0.53f, 0.f), timeToStarvation);

	// RESET PLAYER + CAMERA
	player->setTransform(playerInitTransform);
	player->resetPlayer();
	eagle->setTransform(eagleInitTransform);
	eagle->resetEagle();

	paused = false;
}


void cleanup()
{
	delete textureShader; textureShader = nullptr;
	delete depthMapShader; depthMapShader = nullptr;
	delete debugDepthShader; debugDepthShader = nullptr;
	delete vsmDepthMapShader; vsmDepthMapShader = nullptr;
	delete blurVSMDepthShader; blurVSMDepthShader = nullptr;
	activeShader = nullptr;

	delete textRenderer; textRenderer = nullptr;
	delete particleSystem; particleSystem = nullptr;
	delete ssaoPostprocessor; ssaoPostprocessor = nullptr;

	delete player; player = nullptr;
	delete eagle; eagle = nullptr;
	delete terrain; terrain = nullptr;
	delete cave; cave = nullptr;

	physics->cleanUp();
	delete physics;
}


/**
 * @brief glfw callback function for when the frame buffer (or window) gets resized
 * @param window pointer to active window
 * @param width new framebuffer width
 * @param height new framebuffer height
 */
void frameBufferResize(GLFWwindow *window, int width, int height)
{
	std::cout << "FRAMEBUFFER RESIZED: " << width << ", " << height << std::endl;
	windowWidth = width;
	windowHeight = height;
	glViewport(0, 0, windowWidth, windowHeight);
	ssaoPostprocessor->setupFramebuffers(windowWidth, windowHeight);
}


/**
 * @brief glfw keyCallback on key events
 * @param window pointer to active window
 * @param key the key code of the key that caused the event
 * @param scancode a system and platform specific constant
 * @param action type of key event: GLFW_PRESS, GLFW_RELEASE, GLFW_REPEAT
 * @param mods modifier keys held down on event
 */
void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
	    player->toggleNavMode();
	}

	if (glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS){
		newGame();
		std::cout << "GAME RESTARTED" << std::endl;
	}

	if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
		if (player->attemptDefenseActivation()) {
			particleSystem->respawn(player->getLocation());
			std::cout << "PARTICLE SYSTEM RESPAWNED" << std::endl;
		}
	}

	if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) {
	    debugInfoEnabled = !debugInfoEnabled;
		if (debugInfoEnabled) std::cout << "DEBUG INFO ENABLED" << std::endl;
		else std::cout << "DEBUG INFO DISABLED" << std::endl;
	}

	if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) {
	    wireframeEnabled = !wireframeEnabled;
		if (wireframeEnabled) std::cout << "DRAW WIREFRAME ENABLED" << std::endl;
		else std::cout << "DRAW WIREFRAME DISABLED" << std::endl;
	}

	if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS) {
	    filterType = static_cast<Texture::FilterType>((static_cast<int>(filterType)+3) % 6);

		switch (filterType) {
			case Texture::NEAREST_MIPMAP_OFF:     std::cout << "TEXTURE FILTER NEAREST" << std::endl; break;
			case Texture::NEAREST_MIPMAP_NEAREST: std::cout << "TEXTURE FILTER NEAREST" << std::endl; break;
			case Texture::NEAREST_MIPMAP_LINEAR:  std::cout << "TEXTURE FILTER NEAREST" << std::endl; break;
			case Texture::LINEAR_MIPMAP_OFF:      std::cout << "TEXTURE FILTER LINEAR" << std::endl; break;
			case Texture::LINEAR_MIPMAP_NEAREST:  std::cout << "TEXTURE FILTER LINEAR" << std::endl; break;
			case Texture::LINEAR_MIPMAP_LINEAR:   std::cout << "TEXTURE FILTER LINEAR" << std::endl; break;
		}
	}

	if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS) {
		int filterTypeInt = static_cast<int>(filterType);
	    filterType = static_cast<Texture::FilterType>((static_cast<int>(filterType)+1) % 3 + (filterTypeInt/3)*3);

		switch (filterType) {
			case Texture::NEAREST_MIPMAP_OFF:     std::cout << "MIPMAP OFF" << std::endl; break;
			case Texture::NEAREST_MIPMAP_NEAREST: std::cout << "MIPMAP FILTER NEAREST" << std::endl; break;
			case Texture::NEAREST_MIPMAP_LINEAR:  std::cout << "MIPMAP FILTER LINEAR" << std::endl; break;
			case Texture::LINEAR_MIPMAP_OFF:      std::cout << "MIPMAP OFF" << std::endl; break;
			case Texture::LINEAR_MIPMAP_NEAREST:  std::cout << "MIPMAP FILTER NEAREST" << std::endl; break;
			case Texture::LINEAR_MIPMAP_LINEAR:   std::cout << "MIPMAP FILTER LINEAR" << std::endl; break;
		}
	}

	if (glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS) {
		ssaoEnabled = !ssaoEnabled;
		if (ssaoEnabled) std::cout << "SSAO ENABLED" << std::endl;
		else std::cout << "SSAO DISABLED" << std::endl;
	}

	if (glfwGetKey(window, GLFW_KEY_F7) == GLFW_PRESS) {
		if (!shadowsEnabled) {
			shadowsEnabled = !shadowsEnabled;
			vsmShadowsEnabled = false;
			std::cout << "PCF SHADOWS ENABLED" << std::endl;
		}
		else if (shadowsEnabled) {
			if (vsmShadowsEnabled) {
					shadowsEnabled = !shadowsEnabled;
					std::cout << "SHADOWS DISABLED" << std::endl;
			}
			else {
				vsmShadowsEnabled = true;
				std::cout << "VSM SHADOWS ENABLED" << std::endl;
			}
		}
	}

	if (glfwGetKey(window, GLFW_KEY_F8) == GLFW_PRESS) {
		frustumCullingEnabled = !frustumCullingEnabled;
		if (frustumCullingEnabled) std::cout << "VIEW FRUSTUM CULLING ENABLED" << std::endl;
		else std::cout << "VIEW FRUSTUM CULLING DISABLED" << std::endl;
	}

	if (glfwGetKey(window, GLFW_KEY_F9) == GLFW_PRESS) {
		useAlpha = !useAlpha;
		if (useAlpha) std::cout << "TRANSPARENCY ENABLED" << std::endl;
		else std::cout << "TRANSPARENCY DISABLED" << std::endl;
	}

	if (glfwGetKey(window, GLFW_KEY_F10) == GLFW_PRESS) {
			std::cout << "DEBUG DRAW BULLET PHYSICS NOT IMPLEMENTED" << std::endl;
	}

	if (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS) {
		renderShadowMap = !renderShadowMap;
		if (renderShadowMap) std::cout << "DEBUG DRAW SHADOW MAP ENABLED" << std::endl;
		else std::cout << "DEBUG DRAW SHADOW MAP DISABLED" << std::endl;
	}
}


void setActiveShader(Shader *shader)
{
	activeShader = shader;
	activeShader->useShader();
}


float terrainGetYCoord(glm::vec2 pos2D, float maxDistanceXY, float offsetY)
{
	float newY = 0.0f;

	float x = pos2D.x;
	float z = pos2D.y;
	float lowerX, upperX, lowerZ, upperZ;

	std::vector<Vertex> verts = terrain->getSurface()->getVertices();

	unsigned int i = 0;
	while (i < verts.size()) {
		glm::vec3 vPos = verts.at(i).position; // *glm::mat3(terrain->getMatrix());

		lowerX = vPos.x - maxDistanceXY; upperX = vPos.x + maxDistanceXY;
		lowerZ = vPos.z - maxDistanceXY; upperZ = vPos.z + maxDistanceXY;

		if (x > lowerX && x < upperX) {
			if (z > lowerZ && z < upperZ) {
				newY = vPos.y + offsetY;
				i = verts.size() + 1;
			}
		}

		++i;
	}

	return newY;
}


void initWorldBounds(float &miX, float &maX, float &miZ, float &maZ)
{
	std::vector<Vertex> verts = terrain->getSurface()->getVertices();

	miX = 99999, miZ = 99999;
	maX = -99999, maZ = -99999;

	unsigned int i = 0;
	while (i < verts.size()) {
		glm::vec3 pos = verts.at(i).position;

		if (pos.x > maX) {
			maX = pos.x;
		}
		else if (pos.x < miX) {
			miX = pos.x;
		}

		if (pos.z > maZ) {
			maZ = pos.z;
		}
		else if (pos.z < miZ) {
			miZ = pos.z;
		}

		++i;
	}
	miX += 7; maX -= 7;
	miZ += 7; maZ -= 7;
}

