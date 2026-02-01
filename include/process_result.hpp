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


static cv::Mat process_result(cv::Mat& image, const std::vector<vitis::ai::YOLOv6Result::BoundingBox>& result_in)
{

    // Text size
    double font_scale = 1;
    int thickness = 2;
    int font = cv::FONT_HERSHEY_SIMPLEX;

    /* Print results */
    for (const auto& result : result_in)
    {
        int label = result.label;
        auto& box = result.box;
        float score = result.score;

        // get a color
        cv::Scalar color = getColor(label);
        cv::rectangle(image, cv::Point(box[0], box[1]), cv::Point(box[2], box[3]), color, thickness, 1, 0);

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



inline float overlap(float x1, float w1, float x2, float w2)
{
  float left = std::max(x1 - w1 / 2.0, x2 - w2 / 2.0);
  float right = std::min(x1 + w1 / 2.0, x2 + w2 / 2.0);
  return right - left;
}



inline float cal_iou(std::vector<float> &box, std::vector<float> &truth) 
{
  float w = overlap(box[0], box[2], truth[0], truth[2]);
  float h = overlap(box[1], box[3], truth[1], truth[3]);
  if (w < 0 || h < 0) return 0;

  float inter_area = w * h;
  float union_area = box[2] * box[3] + truth[2] * truth[3] - inter_area;
  return inter_area * 1.0 / union_area;
}



std::vector<vitis::ai::YOLOv6Result::BoundingBox> nms_and_conf(const vitis::ai::YOLOv6Result& result_in, float iou_threshold, float conf_threshold) 
{
  std::vector<vitis::ai::YOLOv6Result::BoundingBox> result_out;

  // Sort by confidence descending
  std::vector<vitis::ai::YOLOv6Result::BoundingBox> sorted = result_in.bboxes;
  std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) {
              return a.score > b.score;
            });

  std::vector<bool> suppressed(sorted.size(), false);

  for (std::size_t i = 0; i < sorted.size(); ++i)
  {
    if (suppressed[i])
        continue;

    if (sorted[i].score < conf_threshold)
        continue;

    result_out.push_back(sorted[i]);

    for (std::size_t j = i + 1; j < sorted.size(); ++j) 
    {
        if (suppressed[j])
            continue;

        // YOLOv6 uses class-aware NMS
        if (sorted[i].label != sorted[j].label)
            continue;

        float iou = cal_iou(sorted[i].box, sorted[j].box);

        if (iou > iou_threshold) 
            suppressed[j] = true;
    }
  }

  return result_out;
}