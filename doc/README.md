## test
- image_enhancements.cpp：实现了图像增强算法
- scale_space：实现了图像金字塔算法，用于图像的尺度空间表示
- hessian_feature_1.cpp：实现了特征点检测
- hessian_feature_2.cpp：在hessian_feature_1.cpp的基础上，增加了特征点描述
- sift_feature_1.cpp：实现了理论的sift特征点检测
- sift_feature_2.cpp：在sift_feature_1.cpp的基础上，增加了特征点描述
- sift_feature_3.cpp：在2的基础上继续测试
- getGuass.cpp：手搓实现了高斯滤波算法

### 调用库函数实现图像拼接，但是没有增加图像融合算法
- sift_correct.cpp：调用opencv的sift特征点检测算法，并且自己实现图像几何校正算法用于矫正图像
- corrct.h：图像几何校正算法的函数声明
- correct.cpp：图像几何校正算法的实现并对图像进行了裁剪

