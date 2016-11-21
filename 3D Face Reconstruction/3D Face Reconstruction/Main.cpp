// ==================================================
// C\C++ INCLUDES
// ==================================================
#include <iostream>
#include <stdio.h>
#include <math.h>

// ==================================================
// OPENCV INCLUDES
// ==================================================
#include <opencv2\opencv.hpp>
#include <opencv2\core.hpp>
#include <opencv2\highgui.hpp>
#include <opencv2\imgproc.hpp>

// ==================================================
// DLIB INCLUDES
// ==================================================
#include <dlib\opencv\cv_image.h>
#include <dlib\image_processing\frontal_face_detector.h>
#include <dlib\image_processing\render_face_detections.h>
#include <dlib\image_processing.h>
#include <dlib\gui_widgets.h>
#include <dlib\image_io.h>

// ==================================================
// GL\SDL\GLM INCLUDES
// ==================================================
#include <GL\glew.h>
#include <SDL2\SDL.h>
#include <glm\glm.hpp>
#include <glm\gtx\transform.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\type_ptr.hpp>
#include <glm\gtx\euler_angles.hpp>
#include <glm\gtx\string_cast.hpp>

#include "OBJLoader.h"

const char* TITLE  = "3D Face Reconstruction";
const int	WIDTH  = 600;
const int	HEIGHT = 600;

const GLchar* VERTEX_SHADER_SOURCE[] = {
	"#version 330 core																				\n"
	"																								\n"
	"layout(location = 0) in vec3 position;															\n"
	"layout(location = 1) in vec3 color;															\n"
	"																								\n"
	"uniform bool calcColor;																		\n"
	"uniform mat4 M;																				\n"
	"uniform mat4 VP;																				\n"
	"																								\n"	
	"out vec2 f_texCoords;																			\n"
	"																								\n"
	"float map(float val, float A, float B, float a, float b)										\n"
	"{																								\n"
	"	return (val - A) * (b - a) / (B - A) + a;													\n"
	"}																								\n"
	"																								\n"
	"void main()																					\n"
	"{																								\n"
	"	vec4 ndc = VP * M * vec4(position, 1);														\n"
	"	gl_Position = ndc;																			\n"
	"	float u = map(ndc.x / ndc.w, -1, 1, 0, 1);													\n"
	"	float v = map(ndc.y / ndc.w, -1, 1, 0, 1);													\n"
	"	f_texCoords = vec2(u, v);																	\n"
	"}																								\n"
};
const GLchar* FRAGMENT_SHADER_SOURCE[] = {
	"#version 330 core																				\n"
	"																								\n"
	"in vec2 f_texCoords;																			\n"
	"																								\n"
	"uniform sampler2D sampler;																		\n"
	"																								\n"
	"out vec4 fragColor;																			\n"
	"																								\n"
	"void main()																					\n"
	"{																								\n"
	"	vec4 sampledColor = texture(sampler, f_texCoords);											\n"
	"	if(sampledColor.r == 0)																		\n"
	"		fragColor = vec4(0.0, 0.8, 0.8, 1.0);													\n"
	"	else																						\n"
	"		fragColor = sampledColor;																\n"
	"}																								\n"
};

//feature			index
//Лево око надвор	36
//Лево око внатре	39
//Десно око надвор	45
//Десно око внатре	42
//Нос лево			31
//Нос десно			35
//Уста лево			48
//Уста десно		54
//Брада				8

// MALE
//const cv::Point3f modelPointsArr[] =
//{
//	cv::Point3f(-1.13642, -0.90731, +0.00391),
//	cv::Point3f(-0.48241, -0.83527, -0.05782),
//	cv::Point3f(+1.13642, -0.90731, +0.00391),
//	cv::Point3f(+0.48241, -0.83527, -0.05782),
//	cv::Point3f(-0.44994, +0.07104, -0.46759),
//	cv::Point3f(+0.44994, +0.07104, -0.46759),
//	cv::Point3f(-0.63738, +0.81876, -0.25444),
//	cv::Point3f(+0.63738, +0.81876, -0.25444),
//	cv::Point3f(-0.00005, +1.85261, -0.27220)
//};

// FEMALE
const cv::Point3f modelPointsArr[] =
{
	cv::Point3f(-1.030300, -0.41930, -0.38129),
	cv::Point3f(-0.493680, -0.38700, -0.55059),
	cv::Point3f(+1.030300, -0.41930, -0.38129),
	cv::Point3f(+0.493680, -0.38700, -0.55059),
	cv::Point3f(-0.363830, +0.52565, -0.79787),
	cv::Point3f(+0.363830, +0.52565, -0.79787),
	cv::Point3f(-0.599530, +1.10768, -0.71667),
	cv::Point3f(+0.599530, +1.10768, -0.71667),
	cv::Point3f(-0.000002, +1.99444, -0.94946)
};

void getTransformationParameters(const std::vector<cv::Point2f>& srcImagePoints, const std::vector<cv::Point3f>& modelPoints,
	const cv::Mat& srcImage, glm::mat3& rotationMatrix, glm::vec3& translationVector);

// ==================================================
//	MAIN
// ==================================================
int main(int argc, char** argv)
{
	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_Window* window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_OPENGL);
	SDL_GLContext glContext = SDL_GL_CreateContext(window);
	glewInit();
	glViewport(0, 0, WIDTH, HEIGHT);
	glClearColor(0.8, 0.8, 0.8, 1);
	glEnable(GL_DEPTH_TEST);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);	

	GLuint vertexShader;
	GLuint fragmentShader;
	GLuint program;
	GLint success;
	GLchar infoLog[512];
	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, VERTEX_SHADER_SOURCE, NULL);
	glCompileShader(vertexShader);
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cerr << "ERROR: Vertex shader compilation failed.\n" << infoLog << std::endl;
	}
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, FRAGMENT_SHADER_SOURCE, NULL);
	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cerr << "ERROR: Fragment shader compilation failed.\n" << infoLog << std::endl;
	}
	program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infoLog);
		std::cerr << "ERROR: Program linking failed.\n" << infoLog << std::endl;
	}
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	IndexedModel mesh = OBJModel("./res/female_face.obj").ToIndexedModel();
	mesh.CalcNormals();
	GLuint numVertices = mesh.positions.size();
	GLuint numIndices = mesh.indices.size();
	GLuint faceVAO;
	GLuint faceVBO;
	GLuint faceEBO;
	glGenVertexArrays(1, &faceVAO);
	glGenBuffers(1, &faceVBO);
	glGenBuffers(1, &faceEBO);
	glBindVertexArray(faceVAO);
	glBindBuffer(GL_ARRAY_BUFFER, faceVBO);
	glBufferData(GL_ARRAY_BUFFER, numVertices * sizeof(glm::vec3), &mesh.positions[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faceEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * sizeof(unsigned int), &mesh.indices[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GL_FLOAT), (GLvoid*)0);
	glBindVertexArray(0);
	
	glm::mat4 model			 = glm::mat4(1.0);
	glm::mat4 view			 = view = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, -1, 0));
	glm::mat4 projection	 = glm::perspective(70.0f, (float)WIDTH / (float)HEIGHT, 0.1f, 1000.0f);
	glm::mat4 viewProjection = projection * view;
	GLuint uMLocation  = glGetUniformLocation(program, "M");
	GLuint uVPLocation = glGetUniformLocation(program, "VP");	
		
	std::vector<cv::Point3f> modelPoints(modelPointsArr, modelPointsArr + sizeof(modelPointsArr) / sizeof(modelPointsArr[0]));

	dlib::frontal_face_detector detector = dlib::get_frontal_face_detector();
	dlib::shape_predictor sp;
	dlib::deserialize("shape_predictor_68_face_landmarks.dat") >> sp;

	cv::Mat srcImageCV;
	dlib::image_window win;

	dlib::array2d<dlib::rgb_pixel> srcImageDLIB;
	std::vector<dlib::rectangle> dets;
	std::vector<dlib::full_object_detection> shapes;
	dlib::full_object_detection shape;

	srcImageCV = cv::imread("./res/face3.jpg");	
	dlib::assign_image(srcImageDLIB, dlib::cv_image<dlib::bgr_pixel>(srcImageCV));
	dets = detector(srcImageDLIB);
	shape = sp(srcImageDLIB, dets[0]);
	std::vector<cv::Point2f> srcImagePoints;
	int i;
	i = 36; srcImagePoints.push_back(cv::Point2f(shape.part(i).x(), shape.part(i).y()));
	i = 39; srcImagePoints.push_back(cv::Point2f(shape.part(i).x(), shape.part(i).y()));
	i = 45; srcImagePoints.push_back(cv::Point2f(shape.part(i).x(), shape.part(i).y()));
	i = 42; srcImagePoints.push_back(cv::Point2f(shape.part(i).x(), shape.part(i).y()));
	i = 31; srcImagePoints.push_back(cv::Point2f(shape.part(i).x(), shape.part(i).y()));
	i = 35; srcImagePoints.push_back(cv::Point2f(shape.part(i).x(), shape.part(i).y()));
	i = 48; srcImagePoints.push_back(cv::Point2f(shape.part(i).x(), shape.part(i).y()));
	i = 54; srcImagePoints.push_back(cv::Point2f(shape.part(i).x(), shape.part(i).y()));
	i = 8;	srcImagePoints.push_back(cv::Point2f(shape.part(i).x(), shape.part(i).y()));
	shapes.push_back(shape);	
	
	win.clear_overlay();
	win.set_image(srcImageDLIB);
	win.add_overlay(dlib::render_face_detections(shapes));
		
	glm::mat3 rotationMatrix;
	glm::vec3 translationVector;
	getTransformationParameters(srcImagePoints, modelPoints, srcImageCV, rotationMatrix, translationVector);
	glm::mat4 translationMatrix = glm::translate(translationVector);
	model = translationMatrix * 
		glm::mat4(
			rotationMatrix[0][0], rotationMatrix[0][1], rotationMatrix[0][2], 0,
			rotationMatrix[1][0], rotationMatrix[1][1], rotationMatrix[1][2], 0,
			rotationMatrix[2][0], rotationMatrix[2][1], rotationMatrix[2][2], 0,
			0,					  0,					0,					  1
		);	
	// std::cout << glm::to_string(translationVector) << std::endl;
	
	GLuint srcImageTexture;
	cv::flip(srcImageCV, srcImageCV, 0);
	glGenTextures(1, &srcImageTexture);
	glBindTexture(GL_TEXTURE_2D, srcImageTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);		
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, srcImageCV.cols, srcImageCV.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, srcImageCV.ptr());
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);	
	
	SDL_Event e;
	boolean run = true;
	float angle = 0.0f;
	while (run)
	{		
		SDL_PollEvent(&e);
		if (e.type == SDL_QUIT)
			run = false;	

		if (e.type == SDL_KEYDOWN)
		{			
			switch (e.key.keysym.sym)
			{
			case SDLK_LEFT:
				angle -= 0.01;
				break;
			case SDLK_RIGHT:
				angle += 0.01;
				break;
			default:
				break;
			}
		}
		glm::mat4 r = glm::rotate(angle, glm::vec3(0, -1, 0));		
		
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(program);
		glProgramUniformMatrix4fv(program, uVPLocation, 1, false, glm::value_ptr(viewProjection));
		glProgramUniformMatrix4fv(program, uMLocation, 1, false, glm::value_ptr(model * r));
				
		glBindTexture(GL_TEXTURE_2D, 0);		
		glBindVertexArray(faceVAO);				
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

		glBindTexture(GL_TEXTURE_2D, srcImageTexture);
		glBindVertexArray(faceVAO);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
				
		SDL_GL_SwapWindow(window);
	}
		
	glDeleteBuffers(1, &faceEBO);
	glDeleteBuffers(1, &faceVBO);
	glDeleteVertexArrays(1, &faceVAO);

	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

// http://www.morethantechnical.com/2012/10/17/head-pose-estimation-with-opencv-opengl-revisited-w-code/
void getTransformationParameters(
	const std::vector<cv::Point2f>& srcImagePoints,
	const std::vector<cv::Point3f>& modelPoints,
	const cv::Mat& srcImage,
	glm::mat3& rotationMatrix,
	glm::vec3& translationVector
	)
{
	std::vector<double> rv(3), tv(3);
	cv::Mat rvec(rv), tvec(tv);	

	cv::Mat ip(srcImagePoints);
	cv::Mat op = cv::Mat(modelPoints);
	cv::Scalar m = mean(cv::Mat(modelPoints));

	rvec = cv::Mat(rv);
	double _d[9] =
	{
		1, 0, 0,
		0, -1, 0,
		0, 0, -1
	};
	Rodrigues(cv::Mat(3, 3, CV_64FC1, _d), rvec);
	tv[0] = 0; tv[1] = 0; tv[2] = 1;
	tvec = cv::Mat(tv);

	double max_d = MAX(srcImage.rows, srcImage.cols);
	double _cm[9] =
	{
		max_d, 0, (double)srcImage.cols / 2.0,
		0, max_d, (double)srcImage.rows / 2.0,
		0, 0, 1.0
	};
	cv::Mat camMatrix = cv::Mat(3, 3, CV_64FC1, _cm);

	double _dc[] = { 0, 0, 0, 0 };
	solvePnP(op, ip, camMatrix, cv::Mat(1, 4, CV_64FC1, _dc), rvec, tvec, false, CV_EPNP);

	double rot[9] = { 0 };
	cv::Mat rotM(3, 3, CV_64FC1, rot);
	Rodrigues(rvec, rotM);
	double* _r = rotM.ptr<double>();
	
	// printf("rotation mat: \n %.3f %.3f %.3f\n%.3f %.3f %.3f\n%.3f %.3f %.3f\n", _r[0], _r[1], _r[2], _r[3], _r[4], _r[5], _r[6], _r[7], _r[8]);
	
	rotM = rotM.t();
	
	rotationMatrix = glm::mat3(
		(float)rot[0], (float)rot[1], (float)rot[2],
		(float)rot[3], (float)rot[4], (float)rot[5],
		(float)rot[6], (float)rot[7], (float)rot[8]);
	
	// printf("trans vec: \n %.3f %.3f %.3f\n", tv[0], tv[1], tv[2]);	

	translationVector = glm::vec3(tv[0], tv[1], tv[2]);

	double _pm[12] =
	{
		_r[0], _r[1], _r[2], tv[0],
		_r[3], _r[4], _r[5], tv[1],
		_r[6], _r[7], _r[8], tv[2]
	};

	cv::Mat tmp, tmp1, tmp2, tmp3, tmp4, tmp5;
	/*
	yaw   y
	pitch x
	roll  z
	*/
	cv::Vec3d eav;
	cv::decomposeProjectionMatrix(cv::Mat(3, 4, CV_64FC1, _pm), tmp, tmp1, tmp2, tmp3, tmp4, tmp5, eav);	
	
	// printf("Face Rotation Angle:  %.5f %.5f %.5f\n", eav[0], eav[1], eav[2]);	
}