## test
- image_enhancements.cpp：实现了图像增强算法
- scale_space：实现了图像金字塔算法，用于图像的尺度空间表示
- hessian_feature_1.cpp：实现了特征点检测
- hessian_feature_2.cpp：在hessian_feature_1.cpp的基础上，增加了特征点描述
- sift_feature_1.cpp：实现了理论的sift特征点检测
- sift_feature_2.cpp：在sift_feature_1.cpp的基础上，增加了特征点描述
- sift_correct.cpp：调用opencv的sift特征点检测算法，并且自己实现图像几何校正算法用于矫正图像
- corrct.h：图像几何校正算法的头文件，同时包含了实现