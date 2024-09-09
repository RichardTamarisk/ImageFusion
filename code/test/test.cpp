#include <iostream>
#include <vector>
#include <optional>
#include <tuple>
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;

class ImageStitcher {
public:
    Mat stitch(const vector<Mat>& images, double ratio = 0.75, double reprojThresh = 4.0, bool showMatches = false) {
        Mat imageA = images[0];
        Mat imageB = images[1];

        // 检测A、B图片的SIFT关键特征点，并计算特征描述子
        auto [kpsA, featuresA] = detectAndDescribe(imageA);
        auto [kpsB, featuresB] = detectAndDescribe(imageB);

        // 匹配两张图片的所有特征点，返回匹配结果
        auto M = matchKeypoints(kpsA, kpsB, featuresA, featuresB, ratio, reprojThresh);

        // 如果返回结果为空，没有匹配成功的特征点，退出算法
        if (!M.has_value()) {
            return Mat();
        }

        // 否则，提取匹配结果
        auto [matches, H, status] = M.value();

        // 将图片A进行视角变换
        Mat result;
        warpPerspective(imageA, result, H, Size(imageA.cols + imageB.cols, imageA.rows));
        result(Rect(0, 0, imageB.cols, imageB.rows)) = imageB;

        // 检测是否需要显示图片匹配
        if (showMatches) {
            Mat vis = drawMatches(imageA, imageB, kpsA, kpsB, matches, status);
            imshow("Keypoint Matches", vis);
        }

        return result;
    }

private:
    tuple<vector<KeyPoint>, Mat> detectAndDescribe(const Mat& image) {
        Mat gray;
        cvtColor(image, gray, COLOR_BGR2GRAY);
        Ptr<SIFT> descriptor = SIFT::create();
        vector<KeyPoint> kps;
        Mat features;
        descriptor->detectAndCompute(gray, noArray(), kps, features);
        return make_tuple(kps, features);
    }

    optional<tuple<vector<DMatch>, Mat, Mat>> matchKeypoints(const vector<KeyPoint>& kpsA, const vector<KeyPoint>& kpsB, const Mat& featuresA, const Mat& featuresB, double ratio, double reprojThresh) {
        BFMatcher matcher(NORM_L2);
        vector<vector<DMatch>> rawMatches;
        matcher.knnMatch(featuresA, featuresB, rawMatches, 2);

        vector<DMatch> matches;
        for (const auto& m : rawMatches) {
            if (m.size() == 2 && m[0].distance < m[1].distance * ratio) {
                matches.push_back(m[0]);
            }
        }

        if (matches.size() > 4) {
            vector<Point2f> ptsA, ptsB;
            for (const auto& match : matches) {
                ptsA.push_back(kpsA[match.queryIdx].pt);
                ptsB.push_back(kpsB[match.trainIdx].pt);
            }
            Mat H = findHomography(ptsA, ptsB, RANSAC, reprojThresh);
            return make_tuple(matches, H, Mat());
        }

        return nullopt;
    }

    Mat drawMatches(const Mat& imageA, const Mat& imageB, const vector<KeyPoint>& kpsA, const vector<KeyPoint>& kpsB, const vector<DMatch>& matches, const Mat& status) {
        Mat vis;
        hconcat(imageA, imageB, vis);
        
        for (const auto& match : matches) {
            Point2f ptA = kpsA[match.queryIdx].pt;
            Point2f ptB = kpsB[match.trainIdx].pt + Point2f(static_cast<float>(imageA.cols), 0);
            line(vis, ptA, ptB, Scalar(0, 255, 0), 1);
        }

        return vis;
    }
};

int main() {
    // 读取拼接图片
    Mat imageA = imread("l.jpg");
    Mat imageB = imread("r.jpg");

    if (imageA.empty() || imageB.empty()) {
        cerr << "Error loading images!" << endl;
        return -1;
    }

    // 创建拼接器实例
    ImageStitcher stitcher;

    // 把图片拼接成全景图
    Mat result = stitcher.stitch({ imageA, imageB }, true);

    if (!result.empty()) {
        imwrite("result.jpg", result);
        // imshow("Result", result);
        waitKey(0);
    } else {
        cout << "Stitching failed!" << endl;
    }

    return 0;
}