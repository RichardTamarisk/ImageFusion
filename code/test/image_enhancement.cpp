#include <iostream>
#include <vector>
#include<opencv2/opencv.hpp>
#include<opencv2/highgui/highgui.hpp>
#include<opencv2/imgproc/imgproc.hpp>
#include <map>
 
#define STEP 6
#define ABS(X) ((X)>0? X:(-(X)))
#define PI 3.1415926
 
using namespace std;
using namespace cv;
  
int main()
{
	Mat srcImage = imread("l.png");
 
	if (srcImage.empty())
	{
		cout << "图像未被读入";
		system("pause");
		return 0;
	}

    // 转为灰度图像
	if (srcImage.channels() != 1)
	{
		cvtColor(srcImage, srcImage, COLOR_BGR2GRAY);
	}
	
  
	int width = srcImage.cols;
	int height = srcImage.rows;
 
    // 创建输入图像
	Mat outImage(height, width, CV_8UC1,Scalar::all(0));
	
    // 定义高斯核的尺寸和标准差
	int W = 5;
	float sigma = 01;
	Mat xxGauKernel(2 * W + 1, 2 * W + 1, CV_32FC1, Scalar::all(0));
	Mat xyGauKernel(2 * W + 1, 2 * W + 1, CV_32FC1, Scalar::all(0));
	Mat yyGauKernel(2 * W + 1, 2 * W + 1, CV_32FC1, Scalar::all(0));
 

    //构建高斯二阶偏导数模板
	// 构建尺度空间：L(x,y,σ) = G(x,y,σ) * I(x,y)
    // 差分高斯(Dog)算子：D(x, y,σ) = L(x, y, kσ) - L(x, y,σ)
	for (int i = -W; i <= W;i++)
	{
		for (int j = -W; j <= W; j++)
		{
			xxGauKernel.at<float>(i + W, j + W) = (1 - (i*i) / (sigma*sigma))*exp(-1 * (i*i + j*j) / (2 * sigma*sigma))*(-1 / (2 * PI*pow(sigma, 4)));
			yyGauKernel.at<float>(i + W, j + W) = (1 - (j*j) / (sigma*sigma))*exp(-1 * (i*i + j*j) / (2 * sigma*sigma))*(-1 / (2 * PI*pow(sigma, 4)));
			xyGauKernel.at<float>(i + W, j + W) = ((i*j))*exp(-1 * (i*i + j*j) / (2 * sigma*sigma))*(1 / (2 * PI*pow(sigma, 6)));
		}
	}
 
 
	for (int i = 0; i < (2 * W + 1); i++)
	{
		for (int j = 0; j < (2 * W + 1); j++)
		{
			cout << xxGauKernel.at<float>(i, j) << "  ";
		}
		cout << endl;
	}
    
    // 对输入图像进行高斯二阶偏导数卷积
	Mat xxDerivae(height, width, CV_32FC1, Scalar::all(0));
	Mat yyDerivae(height, width, CV_32FC1, Scalar::all(0));
	Mat xyDerivae(height, width, CV_32FC1, Scalar::all(0));
    //图像与高斯二阶偏导数模板进行卷积
	filter2D(srcImage, xxDerivae, xxDerivae.depth(), xxGauKernel);
	filter2D(srcImage, yyDerivae, yyDerivae.depth(), yyGauKernel);
	filter2D(srcImage, xyDerivae, xyDerivae.depth(), xyGauKernel);
 
    // 遍历输入图像，计算海森矩阵的特征值并进行图像增强
	for (int h = 0; h < height; h++)
	{
		for (int w = 0; w < width; w++)
		{
			float fxx = xxDerivae.at<float>(h, w);
			float fyy = yyDerivae.at<float>(h, w);
			float fxy = xyDerivae.at<float>(h, w);
 
            // 构建Hessian矩阵
            /*
             H=[Lxy(x,y,σ) Lxy(x,y,σ)]
               [Lxy(x,y,σ) Lxy(x,y,σ)]
            */
			float myArray[2][2] = { { fxx, fxy }, { fxy, fyy } };          //构建矩阵，求取特征值
            Mat Array(2, 2, CV_32FC1, myArray);
			
            // 计算Hessian矩阵的特征值
            Mat eValue;
			Mat eVector;
            eigen(Array, eValue, eVector);                               //矩阵是降序排列的
			
            // 根据特征值进行图像增强
            float a1 = eValue.at<float>(0, 0);
			float a2 = eValue.at<float>(1, 0);
 
			//根据特征向量判断线性结构
			if ((a1>0) && (ABS(a1)>(1+ ABS(a2))))             
			{
				outImage.at<uchar>(h, w) =  pow((ABS(a1) - ABS(a2)), 4);
				// outImage.at<uchar>(h, w) = pow((ABS(a1) / ABS(a2))*(ABS(a1) - ABS(a2)), 1.5);	
			}
 				
		}
 
	}
	Mat element = getStructuringElement(MORPH_RECT, Size(3, 2));
	morphologyEx(outImage, outImage, MORPH_CLOSE, element);
	
	imwrite("image_enhancement.png", outImage);
 
	// imshow("[原始图]", outImage);
	waitKey(0);
 
	return 0;
}