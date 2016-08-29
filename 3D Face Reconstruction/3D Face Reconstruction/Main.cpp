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
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\type_ptr.hpp>
#include <glm\gtx\euler_angles.hpp>

#include "OBJLoader.h"

const char* TITLE  = "3D Face Reconstruction";
const int	WIDTH  = 800;
const int	HEIGHT = 600;

const GLchar* VERTEX_SHADER_SOURCE[] = {
	"#version 330 core										\n"
	"														\n"
	"layout(location = 0) in vec3 position;					\n"
	"layout(location = 1) in vec3 color;					\n"
	"														\n"
	"uniform bool calcColor;								\n"
	"uniform mat4 M;										\n"
	"uniform mat4 VP;										\n"
	"														\n"
	"out vec4 f_color;										\n"
	"														\n"
	"void main()											\n"
	"{														\n"
	"   if(calcColor)										\n"
	"	   f_color = vec4(clamp(position, 0, 1), 1);		\n"
	"	else												\n"
	"	   f_color = vec4(color, 1);						\n"
	"	gl_Position = VP * M * vec4(position, 1);			\n"
	"}														\n"
};
const GLchar* FRAGMENT_SHADER_SOURCE[] = {
	"#version 330 core										\n"
	"														\n"
	"in vec4 f_color;										\n"
	"														\n"
	"out vec4 fragColor;									\n"
	"														\n"
	"void main()											\n"
	"{														\n"
	"	fragColor = f_color;								\n"
	"}														\n"
};

/*
Лево око надвор		37 - 1 (+4.65754, +3.71519, -0.01428)
Лево око внатре		40 - 1 (+1.97604, +3.42160, +0.23674)
Десно око надвор	46 - 1 (-4.65754, +3.71519, -0.01428)
Десно око внатре	43 - 1 (-1.97604, +3.42160, +0.23674)
Нос лево			32 - 1 (+1.84389, -0.29071, +1.91656)
Нос десно			36 - 1 (-1.84389, -0.29071, +1.91656)
Уста лево			49 - 1 (+2.61255, -3.36301, +1.04384)
Уста десно			55 - 1 (-2.61255, -3.36301, +1.04384)
Брада				 9 - 1 (+0.00000, -7.02612, +1.70428)
*/

const cv::Point3f modelPointsArr[] =
{
	cv::Point3f(+4.65754f, +3.71519f, -0.01428f),
	cv::Point3f(+1.97604f, +3.42160f, +0.23674f),
	cv::Point3f(-4.65754f, +3.71519f, -0.01428f),
	cv::Point3f(-1.97604f, +3.42160f, +0.23674f),
	cv::Point3f(+1.84389f, -0.29071f, +1.91656f),
	cv::Point3f(-1.84389f, -0.29071f, +1.91656f),
	cv::Point3f(+2.61255f, -3.36301f, +1.04384f),
	cv::Point3f(-2.61255f, -3.36301f, +1.04384f),
	cv::Point3f(+0.00000f, -7.02612f, +1.70428f)
};

void getEulerAngles(
	const std::vector<cv::Point2f>& srcImagePoints,
	const std::vector<cv::Point3f>& modelPoints,
	const cv::Mat& srcImage,
	cv::Vec3d& eav
	);

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
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

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

	IndexedModel mesh = OBJModel("./res/face.obj").ToIndexedModel();
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
	
	GLfloat axes[] =
	{
		0, 0, 0,	1, 0, 0,
		1, 0, 0,	1, 0, 0,

		0, 0, 0,	0, 1, 0,
		0, 1, 0,	0, 1, 0,

		0, 0, 0,	0, 0, 1,
		0, 0, 1,	0, 0, 1
	};
	GLuint axesVAO;
	GLuint axesVBO;
	glGenVertexArrays(1, &axesVAO);
	glGenBuffers(1, &axesVBO);
	glBindVertexArray(axesVAO);
	glBindBuffer(GL_ARRAY_BUFFER, axesVBO);
	glBufferData(GL_ARRAY_BUFFER, 6 * 6 * sizeof(GL_FLOAT), &axes[0], GL_STATIC_DRAW);	
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GL_FLOAT), (GLvoid*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GL_FLOAT), (GLvoid*)(3 * sizeof(GL_FLOAT)));
	glBindVertexArray(0);

	glm::mat4 model			 = glm::mat4(1.0);
	glm::mat4 view			 = glm::lookAt(glm::vec3(5, 1, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	glm::mat4 projection	 = glm::perspective(70.0f, (float)WIDTH / (float)HEIGHT, 0.1f, 1000.0f);
	glm::mat4 viewProjection = projection * view;
	GLuint uMLocation  = glGetUniformLocation(program, "M");
	GLuint uVPLocation = glGetUniformLocation(program, "VP");
	GLuint uCalcColor  = glGetUniformLocation(program, "calcColor");
		
	std::vector<cv::Point3f> modelPoints(modelPointsArr, modelPointsArr + sizeof(modelPointsArr) / sizeof(modelPointsArr[0]));

	dlib::frontal_face_detector detector = dlib::get_frontal_face_detector();
	dlib::shape_predictor sp;
	dlib::deserialize("shape_predictor_68_face_landmarks.dat") >> sp;

	cv::Mat srcImageCV;
	dlib::image_window win;

	/*
	cv::VideoCapture vc(0);
	if (!vc.isOpened())
	{
		std::cerr << "ERROR: VideoCapture failed to initialize.\n" << std::endl;
		return -1;
	}
	*/

	dlib::array2d<dlib::rgb_pixel> srcImageDLIB;
	std::vector<dlib::rectangle> dets;
	std::vector<dlib::full_object_detection> shapes;
	dlib::full_object_detection shape;
	boolean run = true;
	while (run)
	{
		SDL_Event e;
		SDL_PollEvent(&e);
		if (e.type == SDL_QUIT)
			run = false;

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(program);
		glProgramUniformMatrix4fv(program, uVPLocation, 1, false, glm::value_ptr(viewProjection));

		/*
		vc.read(srcImageCV);	
		*/
		srcImageCV = cv::imread("face.jpg");		
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
		/*
		for (int i = 0; i < srcImagePoints.size(); ++i)
		{
			cv::circle(srcImageCV, srcImagePoints.at(i), 5, cv::Scalar(0, 0, 1));
		}
		cv::imshow("Faces", srcImageCV);
		*/
		win.clear_overlay();
		win.set_image(srcImageDLIB);
		win.add_overlay(dlib::render_face_detections(shapes));
		cv::Vec3d eav;
		getEulerAngles(srcImagePoints, modelPoints, srcImageCV, eav);
		/*
		yaw   y
		pitch x
		roll  z
		*/
		model = glm::eulerAngleYXZ(eav[1], eav[0], eav[2]);		
		glProgramUniformMatrix4fv(program, uMLocation, 1, false, glm::value_ptr(model));		
		glBindVertexArray(faceVAO);
		glUniform1i(uCalcColor, true);		
		glLineWidth(1);
		glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

		model = glm::mat4(1);
		glProgramUniformMatrix4fv(program, uMLocation, 1, false, glm::value_ptr(model));		
		glBindVertexArray(axesVAO);
		glUniform1i(uCalcColor, false);
		glLineWidth(3);
		glDrawArrays(GL_LINES, 0, 12);
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

void getEulerAngles(
	const std::vector<cv::Point2f>& srcImagePoints,
	const std::vector<cv::Point3f>& modelPoints,
	const cv::Mat& srcImage,
	cv::Vec3d& eav
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
	/*
	printf("rotation mat: \n %.3f %.3f %.3f\n%.3f %.3f %.3f\n%.3f %.3f %.3f\n",
	 	_r[0], _r[1], _r[2], _r[3], _r[4], _r[5], _r[6], _r[7], _r[8]);

	printf("trans vec: \n %.3f %.3f %.3f\n", tv[0], tv[1], tv[2]);
	*/

	double _pm[12] =
	{
		_r[0], _r[1], _r[2], tv[0],
		_r[3], _r[4], _r[5], tv[1],
		_r[6], _r[7], _r[8], tv[2]
	};

	cv::Mat tmp, tmp1, tmp2, tmp3, tmp4, tmp5;
	cv::decomposeProjectionMatrix(cv::Mat(3, 4, CV_64FC1, _pm), tmp, tmp1, tmp2, tmp3, tmp4, tmp5, eav);
	/*
	printf("Face Rotation Angle:  %.5f %.5f %.5f\n", eav[0], eav[1], eav[2]);
	*/
}