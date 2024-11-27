#include "../include/stitcher.h"

//************************************
// Method:    avframeToCvmat
// Access:    public
// Returns:   cv::Mat
// Qualifier:
// Parameter: const AVFrame * frame
// Description: AVFrame转MAT
//************************************
cv::Mat avframeToCvmat(const AVFrame *frame) {
    if (!frame) {
        std::cerr << "Frame is null." << std::endl;
        return cv::Mat(); // 返回空 Mat
    }

    int width = frame->width;
    int height = frame->height;

    if (!frame->data[0]) {
        std::cerr << "Invalid AVFrame data." << std::endl;
        return cv::Mat(); // 返回空 Mat
    }

    // 输出格式名称
    std::cout << "AVFrame format: " << av_get_pix_fmt_name((AVPixelFormat)frame->format) << std::endl;

    cv::Mat image(height, width, CV_8UC3);
    int cvLinesizes[1];
    cvLinesizes[0] = image.step[0];

    // 创建转换上下文
    SwsContext *conversion = sws_getContext(
        width, height, (AVPixelFormat)frame->format, width, height,
        AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    
    if (!conversion) {
        std::cerr << "Could not create conversion context." << std::endl;
        return cv::Mat();
    }

    // 使用 sws_scale 转换图像
    int result = sws_scale(conversion, frame->data, frame->linesize, 0, height,
                            reinterpret_cast<uint8_t**>(&image.data), cvLinesizes);
    sws_freeContext(conversion);

    // 检查转换是否成功
    if (result <= 0) {
        std::cerr << "sws_scale failed." << std::endl;
        return cv::Mat(); 
    }

    std::cout << "Converted image size: " << image.size() 
              << ", depth: " << image.depth() << std::endl;

    // 检查深度并转换
    if (image.depth() != CV_8U) {
        cv::Mat image8u;
        image.convertTo(image8u, CV_8U);
        image = image8u; // 更新图像为转换后的版本
    }

    return image;
}

//************************************
// Method:    cvmatToAvframe
// Access:    public
// Returns:   AVFrame *
// Qualifier:
// Parameter: cv::Mat * image
// Parameter: AVFrame * frame
// Description: MAT转AVFrame
//************************************
AVFrame *cvmatToAvframe(const cv::Mat *image, AVFrame *frame) {
    int width = image->cols;
    int height = image->rows;

    // 创建 AVFrame
    if (frame == NULL) {
        frame = av_frame_alloc();
        if (!frame) {
            std::cerr << "Could not allocate AVFrame." << std::endl;
            return nullptr;
        }
    }

    // 为 AVFrame 分配数据
    int ret = av_image_alloc(frame->data, frame->linesize, width, height,
                             AV_PIX_FMT_BGR24, 1);
    if (ret < 0) {
        std::cerr << "Could not allocate image." << std::endl;
        av_frame_free(&frame);
        return nullptr;
    }

    // 设置 AVFrame 的宽高和格式
    frame->width = width;
    frame->height = height;
    frame->format = AV_PIX_FMT_BGR24;

    // 进行颜色空间转换
    SwsContext *conversion = sws_getContext(
        width, height, AV_PIX_FMT_BGR24, 
        width, height, (AVPixelFormat)frame->format, 
        SWS_FAST_BILINEAR, NULL, NULL, NULL);

    if (!conversion) {
        std::cerr << "Could not create sws_context." << std::endl;
        av_frame_free(&frame);
        return nullptr;
    }

    int cvLinesizes[1];
    cvLinesizes[0] = image->step1();
    
    // 转换图像
    sws_scale(conversion, &image->data, cvLinesizes, 0, height, frame->data, frame->linesize);
    sws_freeContext(conversion);
    
    return frame;
}

/**
 * Corrects the lens distortion of an image represented by an AVFrame and crops it.
 *
 * This function takes an input AVFrame, applies lens distortion correction,
 * and crops the resulting image to remove unwanted borders.
 *
 * @param frame_input The input AVFrame containing the image data to be corrected.
 * @param frame_output The output AVFrame that will contain the corrected and cropped image.
 * @return True if the correction and cropping process is successful, false otherwise.
 */
bool correct_image(AVFrame *frame_input, AVFrame *frame_output)
{
    if (!frame_input || !frame_output) {
        return false;
    }
    
    // 将 AVFrame 转换为 cv::Mat
    cv::Mat img = avframeToCvmat(frame_input);

    // 创建输出图像
    cv::Mat drcimg(img.rows, img.cols, CV_8UC3);
    cv::Point lenscenter(img.cols / 2, img.rows / 2);
    cv::Point2f src_a, src_b, src_c, src_d;
    double r, s;
    cv::Point2f mCorrectPoint;
    double distance_to_a_x, distance_to_a_y;

    // 校正每个像素
    for (int row = 0; row < img.rows; row++) {
        for (int cols = 0; cols < img.cols; cols++) {
            r = sqrt((row - lenscenter.y) * (row - lenscenter.y) + (cols - lenscenter.x) * (cols - lenscenter.x)) * 0.56;
            s = 0.9998 - 4.2932 * pow(10, -4) * r + 3.4327 * pow(10, -6) * pow(r, 2) - 2.8526 * pow(10, -9) * pow(r, 3) + 9.8223 * pow(10, -13) * pow(r, 4);mCorrectPoint = cv::Point2f((cols - lenscenter.x) / s * 1.35 + lenscenter.x, (row - lenscenter.y) / s * 1.35 + lenscenter.y);

            // 越界判断
            if (mCorrectPoint.y < 0 || mCorrectPoint.y >= img.rows - 1 || 
                mCorrectPoint.x < 0 || mCorrectPoint.x >= img.cols - 1) {
                continue;
            }
            // 计算临近点的坐标
            src_a = cv::Point2f(static_cast<int>(mCorrectPoint.x), static_cast<int>(mCorrectPoint.y));
            src_b = cv::Point2f(src_a.x + 1, src_a.y);
            src_c = cv::Point2f(src_a.x, src_a.y + 1);
            src_d = cv::Point2f(src_a.x + 1, src_a.y + 1);
            // 计算新的像素值
            distance_to_a_x = mCorrectPoint.x - src_a.x;
            distance_to_a_y = mCorrectPoint.y - src_a.y;
            for (int channel = 0; channel < 3; channel++) {
                drcimg.at<cv::Vec3b>(row, cols)[channel] =
                    img.at<cv::Vec3b>(src_a.y, src_a.x)[channel] * (1 - distance_to_a_x) * (1 - distance_to_a_y) +
                    img.at<cv::Vec3b>(src_b.y, src_b.x)[channel] * distance_to_a_x * (1 - distance_to_a_y) +
                    img.at<cv::Vec3b>(src_c.y, src_c.x)[channel] * distance_to_a_y * (1 - distance_to_a_x) +
                    img.at<cv::Vec3b>(src_d.y, src_d.x)[channel] * distance_to_a_y * distance_to_a_x;
            }
        }
    }

    // 裁剪
    int topCropHeight = drcimg.rows * 0.126;
    int bottomCropHeight = drcimg.rows * 0.126;
    int leftCropWidth = drcimg.cols * 0.083;
    int rightCropWidth = drcimg.cols * 0.085;
    cv::Rect roi(leftCropWidth, topCropHeight, drcimg.cols - leftCropWidth - rightCropWidth, drcimg.rows - topCropHeight - bottomCropHeight);
    cv::Mat croppedImg = drcimg(roi);

    // 转换为 AVFrame
    cvmatToAvframe(&croppedImg, frame_output);

    return true;
}

/**
 * Fuses two AVFrames into a single fused frame.
 *
 * This function takes two input AVFrames, frame1 and frame2, and fuses them into a single output frame, frame_fused.
 * The fusion process is performed by averaging the pixel values of the two input frames.
 * The behavior of the fusion process can be controlled by the options parameter, which specifies whether to apply
 * lens distortion correction before fusion.
 *
 * @param frame1 The first input AVFrame.
 * @param frame2 The second input AVFrame.
 * @param frame_fused The output AVFrame that will contain the fused image.
 * @param is_correct The boolean value will determine whether to perform image correction
 * @return True if the fusion process is successful, false otherwise.
 */
bool image_fusion(AVFrame *frame1, AVFrame *frame2, AVFrame *frame_fused, bool is_correct) {
    // 检查输入帧有效性
    if (!frame1 || !frame2 || !frame_fused) {
        return false;
    }

    cv::Mat img1, img2;

    // 根据 is_correct 决定是否进行几何校正
    if (is_correct) {
        AVFrame* frame_corrected1 = av_frame_alloc();
        AVFrame* frame_corrected2 = av_frame_alloc();

        if (!frame_corrected1 || !frame_corrected2) {
            return false; 
        }

        // 进行图像几何校正
        if (!correct_image(frame1, frame_corrected1) || !correct_image(frame2, frame_corrected2)) {
            av_frame_free(&frame_corrected1);
            av_frame_free(&frame_corrected2);
            return false;
        }
    } else {
        // 转换输入帧为 cv::Mat
        img1 = avframeToCvmat(frame1);
        img2 = avframeToCvmat(frame2);
    }
    // 创建 SIFT 特征检测器
    cv::Ptr<cv::SIFT> detector = cv::SIFT::create(1000);
    std::vector<cv::KeyPoint> keypoints1, keypoints2;
    cv::Mat descriptors1, descriptors2;

    detector->detectAndCompute(img1, cv::Mat(), keypoints1, descriptors1);
    detector->detectAndCompute(img2, cv::Mat(), keypoints2, descriptors2);

    // 创建暴力匹配器并进行描述子匹配
    cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create(cv::NORM_L2);
    std::vector<std::vector<cv::DMatch>> matches;
    matcher->knnMatch(descriptors1, descriptors2, matches, 2);

    // 筛选出良好的匹配点对
    std::vector<cv::DMatch> good_matches;
    for (const auto& match : matches) {
        if (match[0].distance < 0.7 * match[1].distance) {
            good_matches.push_back(match[0]);
        }
    }

    // 提取匹配的关键点
    std::vector<cv::Point2f> points1, points2;
    for (const auto& match : good_matches) {
        points1.push_back(keypoints1[match.queryIdx].pt);
        points2.push_back(keypoints2[match.trainIdx].pt);
    }

    // 使用 RANSAC 算法计算透视变换
    cv::Mat homography = cv::findHomography(points2, points1, cv::RANSAC, 5.0);
    if (homography.empty()) {
        return false; // 透视变换失败
    }

    // 拼接图像
    int dst_width = img1.cols + img2.cols;
    int dst_height = std::max(img1.rows, img2.rows);
    cv::Mat dst = cv::Mat::zeros(dst_height, dst_width, CV_8UC3);

    // 将图像 1 复制到目标图像
    img1.copyTo(dst(cv::Rect(0, 0, img1.cols, img1.rows)));

    // 将图像 2 透视变换并复制到目标图像
    cv::Mat transformed_img2;
    cv::warpPerspective(img2, transformed_img2, homography, dst.size());

    // 自动识别重叠区域
    std::vector<cv::Point2f> corners = {
        cv::Point2f(0, 0),
        cv::Point2f(img1.cols, 0),
        cv::Point2f(img1.cols, img1.rows),
        cv::Point2f(0, img1.rows)
    };

    std::vector<cv::Point2f> transformed_corners;
    cv::perspectiveTransform(corners, transformed_corners, homography);
    cv::Rect transformed_rect = cv::boundingRect(transformed_corners);
    cv::Rect overlap_rect = cv::Rect(0, 0, img1.cols, img1.rows) & transformed_rect;

    // 进行渐入渐出法处理重叠区域
    for (int y = overlap_rect.y; y < overlap_rect.y + overlap_rect.height; y++) {
        for (int x = overlap_rect.x; x < overlap_rect.x + overlap_rect.width; x++) {
            // 计算权重
            float d1 = static_cast<float>(overlap_rect.x + overlap_rect.width - x) / overlap_rect.width; 
            float d2 = static_cast<float>(x - overlap_rect.x) / overlap_rect.width;
            // 确保权重在 [0, 1] 之间
            d1 = std::clamp(d1, 0.0f, 1.0f);
            d2 = std::clamp(d2, 0.0f, 1.0f);
            // 获取左图和右图的像素
            cv::Vec3b pixel1 = img1.at<cv::Vec3b>(y, x);
            cv::Vec3b pixel2 = transformed_img2.at<cv::Vec3b>(y, x);
            // 进行加权平均（逐通道处理）
            cv::Vec3b blended_pixel;
            blended_pixel[0] = cv::saturate_cast<uchar>(pixel1[0] * d1 + pixel2[0] * d2);
            blended_pixel[1] = cv::saturate_cast<uchar>(pixel1[1] * d1 + pixel2[1] * d2);
            blended_pixel[2] = cv::saturate_cast<uchar>(pixel1[2] * d1 + pixel2[2] * d2);
            // 更新拼接后图像的重叠区域像素
            dst.at<cv::Vec3b>(y, x) = blended_pixel;
        }
    }
    // 处理右图的非重叠部分
    cv::Rect right_non_overlap_rect(overlap_rect.x + overlap_rect.width, 0, transformed_img2.cols - overlap_rect.x - overlap_rect.width, transformed_img2.rows);
    if (right_non_overlap_rect.width > 0 && right_non_overlap_rect.x < dst.cols) {
        transformed_img2(right_non_overlap_rect).copyTo(dst(right_non_overlap_rect));
    }
    
    // 转换拼接后的图像为 AVFrame
    cvmatToAvframe(&dst, frame_fused);

    return true;
}