#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <map>

#define STEP 6
#define ABS(X) ((X)>0? X:(-(X)))
#define PI 3.1415926

// 寻找特征点
std::vector<cv::Point2f> findFeaturePoints(cv::Mat& srcImage, float threshold) 
{
    std::vector<cv::Point2f> featurePoints;
    int width = srcImage.cols;
    int height = srcImage.rows;

    // 定义高斯核的尺寸和标准差
    int W = 10                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ;
    float sigma = 1.0;
    cv::Mat xxGauKernel(2 * W + 1, 2 * W + 1, CV_32FC1, cv::Scalar::all(0));
    cv::Mat xyGauKernel(2 * W + 1, 2 * W + 1, CV_32FC1, cv::Scalar::all(0));
    cv::Mat yyGauKernel(2 * W + 1, 2 * W + 1, CV_32FC1, cv::Scalar::all(0));

    // 构建高斯二阶偏导数模板
    for (int i = -W; i <= W; i++) 
    {
        for (int j = -W; j <= W; j++) 
        {
            xxGauKernel.at<float>(i + W, j + W) = (1 - (i * i) / (sigma * sigma)) * exp(-1 * (i * i + j * j) / (2 * sigma * sigma)) * (-1 / (2 * PI * pow(sigma, 2)));
            yyGauKernel.at<float>(i + W, j + W) = (1 - (j * j) / (sigma * sigma)) * exp(-1 * (i * i + j * j) / (2 * sigma * sigma)) * (-1 / (2 * PI * pow(sigma, 2)));
            xyGauKernel.at<float>(i + W, j + W) = ((i * j)) * exp(-1 * (i * i + j * j) / (2 * sigma * sigma)) * (1 / (2 * PI * pow(sigma, 4)));
        }
    }

    // 对输入图像进行高斯二阶偏导数卷积
    cv::Mat xxDerivae(height, width, CV_32FC1, cv::Scalar::all(0));
    cv::Mat yyDerivae(height, width, CV_32FC1, cv::Scalar::all(0));
    cv::Mat xyDerivae(height, width, CV_32FC1, cv::Scalar::all(0));
    cv::filter2D(srcImage, xxDerivae, xxDerivae.depth(), xxGauKernel);
    cv::filter2D(srcImage, yyDerivae, yyDerivae.depth(), yyGauKernel);
    cv::filter2D(srcImage, xyDerivae, xyDerivae.depth(), xyGauKernel);

    // 遍历输入图像,寻找Hessian矩阵的局部最大值作为特征点
    for (int h = W; h < height - W; h++)
    {
        for (int w = W; w < width - W; w++) 
        {
            float fxx = xxDerivae.at<float>(h, w);
            float fyy = yyDerivae.at<float>(h, w);
            float fxy = xyDerivae.at<float>(h, w);

            // 构建Hessian矩阵
            float myArray[2][2] = { { fxx, fxy }, { fxy, fyy } };
            cv::Mat Array(2, 2, CV_32FC1, myArray);

            // 计算Hessian矩阵的特征值
            cv::Mat eValue;
            cv::Mat eVector;
            eigen(Array, eValue, eVector);

            // 根据特征值判断是否为局部最大值
            float a1 = eValue.at<float>(0, 0);
            float a2 = eValue.at<float>(1, 0);
            if ((a1 > 0) && (a2 > 0) && (a1 > threshold) && (a2 > threshold))
            {
                featurePoints.push_back(cv::Point2f(w, h));
            }
        }
    }

    return featurePoints;
}

// 计算特征点方向
void computeFeaturePointDirection(cv::Mat& srcImage, std::vector<cv::Point2f>& featurePoints, std::vector<float>& directions) 
{
    for (int i = 0; i < featurePoints.size(); i++) 
    {
        float x = featurePoints[i].x;
        float y = featurePoints[i].y;
        
        // 计算特征点周围区域的梯度幅值和方向
        float maxMagnitude = 0.0f;
        float mainDirection = 0.0f;
        for (int dx = -STEP; dx <= STEP; dx++) 
        {
            for (int dy = -STEP; dy <= STEP; dy++) 
            {
                int xx = x + dx;
                int yy = y + dy;
                if (xx >= 0 && xx < srcImage.cols && yy >= 0 && yy < srcImage.rows) 
                {
                    float gx = srcImage.at<float>(yy, xx + 1) - srcImage.at<float>(yy, xx - 1);
                    float gy = srcImage.at<float>(yy + 1, xx) - srcImage.at<float>(yy - 1, xx);
                    float magnitude = sqrt(gx * gx + gy * gy);
                    float direction = atan2(gy, gx) * 180 / PI;
                    if (magnitude > maxMagnitude) 
                    {
                        maxMagnitude = magnitude;
                        mainDirection = direction;
                    }
                }
            }
        }
        
        // 将特征点的方向信息存储在特征点对象中
        featurePoints[i].x = x;
        featurePoints[i].y = y;
        // 存储特征点的主方向
        directions.push_back(mainDirection);
    }
}

// 描述特征点
void computeFeatureDescriptors(cv::Mat& srcImage, std::vector<cv::Point2f>& featurePoints, std::vector<float>& featureDescriptors)
{
    // 设置特征描述子的patch尺寸和直方图bin的数量
    int patchSize = 16;
    int histogramBins = 8;

    // 遍历特征点
    for (size_t i = 0; i < featurePoints.size(); i++) 
    {
        // 获取当前特征点的x、y坐标和方向
        float x = featurePoints[i].x;
        float y = featurePoints[i].y;
        float dir = featureDescriptors[i];

        // 创建当前特征点的描述子向量
        cv::Mat descriptor(1, histogramBins * histogramBins, CV_32F, cv::Scalar::all(0));

        // 计算特征点周围区域的梯度直方图
        for (int dx = -patchSize / 2; dx < patchSize / 2; dx++) 
        {
            for (int dy = -patchSize / 2; dy < patchSize / 2; dy++) 
            {
                // 计算相对于特征点的像素坐标
                int xx = x + dx;
                int yy = y + dy;

                // 检查像素坐标是否在图像范围
                if (xx >= 0 && xx < srcImage.cols && yy >= 0 && yy < srcImage.rows) 
                {
                    // 计算x和y方向的梯度
                    float gx = srcImage.at<float>(yy, xx + 1) - srcImage.at<float>(yy, xx - 1);
                    float gy = srcImage.at<float>(yy + 1, xx) - srcImage.at<float>(yy - 1, xx);
                    float magnitude = sqrt(gx * gx + gy * gy);
                    float direction = atan2(gy, gx) * 180 / PI - dir;
                    if (direction < 0) direction += 360;

                    // 将梯度信息累积到对应的直方图bin中
                    int histX = (dx + patchSize / 2) / (patchSize / histogramBins);
                    int histY = (dy + patchSize / 2) / (patchSize / histogramBins);
                    int histIndex = histY * histogramBins + histX;
                    descriptor.at<float>(0, histIndex) += magnitude;
                }
            }
        }

        // 对descriptor进行归一化
        float norm = cv::norm(descriptor, cv::NORM_L2);
        descriptor /= norm;

        // 将特征点及其描述子加入到结果中
        cv::Point2f kp(x, y);
        featurePoints.push_back(kp);
        featureDescriptors.push_back(descriptor.at<float>(0));
    }
}


// 绘画特征点
void drawFeaturePointsWithDirection(cv::Mat& srcImage, std::vector<cv::Point2f>& featurePoints, std::vector<float>& directions)
{
    cv::Mat colorImage;
    srcImage.copyTo(colorImage);

    for (size_t i = 0; i < featurePoints.size(); i++) 
    {
        float x = featurePoints[i].x;
        float y = featurePoints[i].y;
        float dir = directions[i];

        // 绘制特征点
        cv::circle(colorImage, featurePoints[i], 3, cv::Scalar(0, 0, 255), 2);

        // 绘制特征点方向
        float dx = cos(dir * PI / 180.0f) * 10;
        float dy = sin(dir * PI / 180.0f) * 10;
        cv::line(colorImage, cv::Point2f(x, y), cv::Point2f(x + dx, y + dy), cv::Scalar(255, 0, 0), 2);
    }

    // imshow("Feature Points with Direction", colorImage);
    imwrite("hessian_feature_2_result.png", colorImage);
    // cv::waitKey(0);
}

int main() 
{
    cv::Mat srcImage = cv::imread("l.png");

    if (srcImage.empty()) 
    {
        std::cout << "图像未被读入";
        system("pause");
        return 0;
    }

    // 寻找特征点
    std::vector<cv::Point2f> featurePoints = findFeaturePoints(srcImage, 4.0f);

    // 计算特征点方向
    std::vector<float> directions;
    computeFeaturePointDirection(srcImage, featurePoints, directions);

    // 描绘特征点
    drawFeaturePointsWithDirection(srcImage, featurePoints, directions);

    // 最后计算特征点描述子
    computeFeatureDescriptors(srcImage, featurePoints, directions);

    // system("pause");
    return 0;
}



    
