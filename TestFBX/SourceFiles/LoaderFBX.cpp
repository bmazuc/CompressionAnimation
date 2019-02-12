
// Directive de compilation 'pragma' 
// ici force le linkage avec la lib specifiee en parametre
#if _WIN32
#define FREEGLUT_LIB_PRAGMAS	1
#pragma comment (lib, "freeglut.lib")
#pragma comment (lib, "glew32.lib")
#pragma comment (lib, "opengl32.lib")
#endif

#include <iostream>
#include <cassert>

#include "glew.h"
#include "freeglut.h"

#include "GLShader.h"

GLShader BlitProgram;
GLuint FBO;
GLuint TexFBO;
GLuint DepthFBO;

bool CreateFramebuffer(int width, int height)
{
	// le renderbuffer se distingue de la texture car en ecriture seule
	glGenRenderbuffers(1, &DepthFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, DepthFBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);

	// creation de la texture qui va servir de color buffer pour le rendu hors ecran
	glGenTextures(1, &TexFBO);
	glBindTexture(GL_TEXTURE_2D, TexFBO);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	// creation du framebuffer pour le rendu hors ecran
	glGenFramebuffers(1, &FBO);
	// GL_FRAMEBUFFER = read/write, GL_READ_FRAMEBUFFER = read only, GL_DRAW_FRAMEBUFFER = write only
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	// attache la texture en tant que color buffer #0
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, TexFBO, 0);
	// attache le render buffer en tant que depth buffer
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DepthFBO);
	// Switch vers le framebuffer par defaut (celui cree par GLUT ou le contexte de rendu)
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	// necessite #include <cassert>
	assert(status == GL_FRAMEBUFFER_COMPLETE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	return true;
}

void DestroyFramebuffer()
{
	glDeleteRenderbuffers(1, &DepthFBO);
	glDeleteFramebuffers(1, &FBO);
	glDeleteTextures(1, &TexFBO);
}

GLShader BasicProgram;

#include "MeshFBX.h"

LoaderFBX g_Scene;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


GLuint LoadAndCreateTexture(const char* fullpath, bool linear = false)
{
	GLuint texture_id = 0;
	int w, h, comp;

	unsigned char* image = stbi_load(fullpath, &w, &h, &comp, STBI_default);
	if (!image) {
		std::cerr << "Unable to load texture: " << fullpath << std::endl;
		return 0;
	}
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	if (comp == 3) {
		glTexImage2D(GL_TEXTURE_2D, 0, !linear ? GL_RGB8 : GL_SRGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	}
	else if (comp == 4) {
		glTexImage2D(GL_TEXTURE_2D, 0, !linear ? GL_RGBA8 : GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(image);

	return texture_id;
}

void Initialize()
{
	glewInit();

	glEnable(GL_FRAMEBUFFER_SRGB);

	// extension .v, .vs, .vert, *.glsl ou autre
	BasicProgram.LoadShader(GL_VERTEX_SHADER, "../Assets/Shaders/DirectionalLight.vs");
	// extension .f, .fs, .frag, *.glsl ou autre
	BasicProgram.LoadShader(GL_FRAGMENT_SHADER, "../Assets/Shaders/DirectionalLight.fs");
	BasicProgram.CreateProgram(true);

	g_Scene.Initialize();
	g_Scene.LoadScene("../Assets/SimpleQuantizationCompression.FBX");
}

void Shutdown()
{
	g_Scene.Shutdown();
	BasicProgram.Destroy();
}

void Update() {
	glutPostRedisplay();
}
int key = 0;
int delta = 0;
void Render() {
	glEnable(GL_DEPTH_TEST);

	glClearColor(0.50f, 0.50f, 0.50f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	BasicProgram.Bind();
	auto programID = BasicProgram.GetProgram();
	
	float timeInSeconds = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
	auto timeLocation = glGetUniformLocation(programID, "u_Time");
	static float previousTime = 0.0f;
	float deltaTime = previousTime - timeInSeconds;
	previousTime = timeInSeconds;
	static float rotationSpeed;
	static float speed = 36.0f;
	rotationSpeed += deltaTime * speed;
	rotationSpeed = fmod(rotationSpeed, 360.0f);
	glUniform1f(timeLocation, timeInSeconds);
	
	Keyframe* currentKey = nullptr;
	Keyframe* nextKey = nullptr;
	float mat[16*53];

	std::vector<float> matrix;
	for (int i = 0; i < 53; i++)
	{  
		currentKey = g_Scene.mSkeleton.m_Joints[i].animation->next;
		nextKey = currentKey;

		for (int i2 = 0; i2 <= key; i2++)
		{
			if (currentKey->next != nullptr)
			{
				currentKey = currentKey->next;

				if (nextKey->next != nullptr)
					nextKey = currentKey->next;
				else
				{
					nextKey = g_Scene.mSkeleton.m_Joints[i].animation->next;
				}
			}
			else
			{
				key = 0;
				currentKey = g_Scene.mSkeleton.m_Joints[i].animation->next;
				nextKey = currentKey->next;
			}
		}
		//FbxMatrix offseMatrix = g_Scene.mSkeleton.m_Joints[i].animation->globalTransform.Slerp(nextOffset,1);
	//	FbxAMatrix finale = currentKey->globalTransform * g_Scene.mSkeleton.m_Joints[i].globalBindPoseInverse;
		
//		FbxAMatrix finale = currentKey->globalTransform * g_Scene.mSkeleton.m_Joints[i].globalBindPoseInverse;
		
		FbxAMatrix currentPos = currentKey->globalTransform;
		FbxAMatrix nextPos = nextKey->globalTransform;
		FbxAMatrix TPose = g_Scene.mSkeleton.m_Joints[i].globalBindPoseInverse;
		
		currentPos.Slerp(nextPos, (float)(delta / 24.0f));
		FbxAMatrix finale = currentPos * TPose;

		//FbxAMatrix finale =  g_Scene.mSkeleton.m_Joints[i].globalBindPoseInverse * g_Scene.mSkeleton.m_Joints[i].animation->globalTransform;
		//FbxAMatrix finale =  g_Scene.mSkeleton.m_Joints[i].globalBindPoseInverse;
		//finale = finale.Inverse();
		
		for (int i = 0; i < 4; ++i)
		{
			matrix.push_back((float)finale.GetRow(i).mData[0]);
			matrix.push_back((float)finale.GetRow(i).mData[1]);
			matrix.push_back((float)finale.GetRow(i).mData[2]);
			matrix.push_back((float)finale.GetRow(i).mData[3]);
		}
	}

	auto skinLocation = glGetUniformLocation(programID, "skin");
	if (skinLocation != -1)
	{
		glUniformMatrix4fv(skinLocation, 53, GL_FALSE, matrix.data());
	}
	
	for (auto& mesh : g_Scene.m_Meshes)
	{
		if (!mesh.materials.empty())
		{
			if (mesh.materials[0].hasDiffuse)
			{
				glUniform1i(glGetUniformLocation(programID, "u_DiffuseTexture"), 0);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, mesh.materials[0].TextureDiffuse);
			}
			if (mesh.materials[0].hasSpecular)
			{
				glUniform1i(glGetUniformLocation(programID, "u_SpecularTexture"), 1);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, mesh.materials[0].TextureSpecular);

			}
			if (mesh.materials[0].hasNormal)
			{
				glActiveTexture(GL_TEXTURE2);
				glUniform1i(glGetUniformLocation(programID, "u_NormalTexture"), 2);
				glBindTexture(GL_TEXTURE_2D, mesh.materials[0].TextureNormal);
			}
		}
		

		mesh.Draw();
	}

	glBindVertexArray(0);

	glutSwapBuffers();	

	static float speedX = 12.0f;
	if (delta >= 23.0f)
	{
		++key;
		delta = 0.0f;
	}
	else
	{
		delta += speedX * 0.416f;
	}
	//key++;
}

void Resize(int, int ) {}

int main(int argc, char* argv[])
{
	glutInit(&argc, argv);
	glutInitWindowSize(1280, 720);
	glutInitWindowPosition(100, 100);
	// defini les buffers du framebuffer 
	// GLUT_RGBA :  color buffer 32 bits
	// GLUT_DOUBLE : double buffering
	// GLUT_DEPTH : ajoute un buffer pour le depth test
	glutInitDisplayMode(GLUT_SRGB | GLUT_ALPHA | GLUT_DEPTH | GLUT_DOUBLE);

	glutCreateWindow("Loader FBX");
	
	Initialize();
	
	glutReshapeFunc(Resize);
	glutIdleFunc(Update);
	glutDisplayFunc(Render);
#if FREEGLUT
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE
			, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
#endif
	glutMainLoop();

	Shutdown();

	return 1;
}