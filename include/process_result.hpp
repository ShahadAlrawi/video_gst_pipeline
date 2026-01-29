#pragma once
#include <iomanip>
#include <iostream>
#include <opencv2/core.hpp>
#include "coco_classes.hpp"

static cv::Scalar getColor(int label) 
{
    static const int palette[3] = { 37, 17, 29 };
    int r = (palette[0] * label) % 255;
    int g = (palette[1] * label) % 255;
    int b = (palette[2] * label) % 255;
    return cv::Scalar(b, g, r);
}


static cv::Mat process_result(cv::Mat& image, const vitis::ai::YOLOv6Result& result_in, bool print_result)
{
    /* Print results */
    for (const auto& result : result_in.bboxes)
    {
        int label = result.label;
        auto& box = result.box;

        // get a color
        cv::Scalar color = getColor(label);

        LOG_IF(INFO, print_result) << "RESULT: " << coco_labels[label] << "\t" << std::fixed
                                << std::setprecision(2) << box[0] << "\t" << box[1]
                                << "\t" << box[2] << "\t" << box[3] << "\t"
                                << std::setprecision(6) << result.score << "\n";

        cv::rectangle(image, cv::Point(box[0], box[1]), cv::Point(box[2], box[3]), color, 1, 1, 0);


         // Text size
        int baseline = 0;
        double font_scale = 0.5;
        int thickness = 1;
        int font = cv::FONT_HERSHEY_SIMPLEX;
        
        // Draw label
        cv::putText(image, coco_labels[label],
                    cv::Point(box[0], box[1]),
                    font, 
                    font_scale,
                    color,
                    thickness);
    }
    return image;
}