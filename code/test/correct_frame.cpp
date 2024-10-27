#include "correct.h"

void correctedImage(cv::Mat& input, cv::Mat& output) 
{
    clock_t start_time = clock();
    
    // 使用输入的图像
    Mat drcimg(input.rows, input.cols, CV_8UC3);
    Point lenscenter(input.cols / 2, input.rows / 2); // 镜头中心在图像中的位置
    Point2f src_a, src_b, src_c, src_d; // a、b、c、d四个顶点
    double r; // 矫正前像素点跟镜头中心的距离
    double s; // 矫正后像素点跟镜头中心的距离
    Point2f mCorrectPoint; // 矫正后点坐标
    double distance_to_a_x, distance_to_a_y; // 求得中心点和边界的距离

    for (int row = 0; row < input.rows; row++) 
    {
        for (int cols = 0; cols < input.cols; cols++) 
        {
            // 计算当前像素点到图像中心点的距离
            r = sqrt((row - lenscenter.y) * (row - lenscenter.y) + (cols - lenscenter.x) * (cols - lenscenter.x)) * 0.75;
            // 根据距离 r 计算比例因子 s
            s = 0.9998 - 4.2932 * pow(10, -4) * r + 3.4327 * pow(10, -6) * pow(r, 2) - 2.8526 * pow(10, -9) * pow(r, 3) + 9.8223 * pow(10, -13) * pow(r, 4);
            // 计算修正后的像素点位置
            mCorrectPoint = Point2f((cols - lenscenter.x) / s * 1.35 + lenscenter.x, (row - lenscenter.y) / s * 1.35 + lenscenter.y);
            // 越界判断
            if (mCorrectPoint.y < 0 || mCorrectPoint.y >= input.rows - 1 || mCorrectPoint.x < 0 || mCorrectPoint.x >= input.cols - 1)
            {
                continue;
            }

            // 计算临近点的坐标
            src_a = Point2f((int)mCorrectPoint.x, (int)mCorrectPoint.y);
            src_b = Point2f(src_a.x + 1, src_a.y);
            src_c = Point2f(src_a.x, src_a.y + 1);
            src_d = Point2f(src_a.x + 1, src_a.y + 1);

            // 计算当前像素点的新像素值
            distance_to_a_x = mCorrectPoint.x - src_a.x;
            distance_to_a_y = mCorrectPoint.y - src_a.y;

            // 使用双线性插值计算新的像素值
            for (int c = 0; c < 3; ++c) {
                drcimg.at<Vec3b>(row, cols)[c] =
                    input.at<Vec3b>(src_a.y, src_a.x)[c] * (1 - distance_to_a_x) * (1 - distance_to_a_y) +
                    input.at<Vec3b>(src_b.y, src_b.x)[c] * distance_to_a_x * (1 - distance_to_a_y) +
                    input.at<Vec3b>(src_c.y, src_c.x)[c] * distance_to_a_y * (1 - distance_to_a_x) +
                    input.at<Vec3b>(src_d.y, src_d.x)[c] * distance_to_a_y * distance_to_a_x;
            }
        }
    }

    // 将drcimg转换为cv::Mat对象
    drcimg.convertTo(output, CV_8UC3);

    // // 裁剪
    // int topCropHeight = drcimg.rows * 0.126; // 上方裁剪
    // int bottomCropHeight = drcimg.rows * 0.126; // 下方裁剪
    // int leftCropWidth = drcimg.cols * 0.07; // 左侧裁剪
    // int rightCropWidth = drcimg.cols * 0.072; // 右侧裁剪

    // cv::Rect roi(leftCropWidth, topCropHeight, drcimg.cols - leftCropWidth - rightCropWidth, drcimg.rows - topCropHeight - bottomCropHeight);
    // cv::Mat croppedImg = drcimg(roi);
    // output = croppedImg;

    cv::Mat drcimg_output;
    drcimg.convertTo(drcimg_output, CV_8UC3);
    output = drcimg_output;


    double elapsed_time = double(clock() - start_time) / CLOCKS_PER_SEC;
    std::cout << "Elapsed time: " << elapsed_time << " seconds" << std::endl;
}