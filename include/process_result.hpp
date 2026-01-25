/*
 * Copyright 2022-2023 Advanced Micro Devices Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once
#include <iomanip>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

/**
 * @brief load kinds from file to a vector
 *
 * @param path - path of the kinds file
 * @param kinds - the vector of kinds string
 *
 * @return none
 */
static void LoadWords(std::string const& path, std::vector<std::string>& kinds)
{
    kinds.clear();

    std::ifstream fkinds(path);
    if (fkinds.fail()) {
        fprintf(stderr, "Error : Open %s failed.\n", path.c_str());
        exit(1);
    }

    std::string kind;
    while (getline(fkinds, kind))
    {
        kinds.push_back(kind);
    }

    fkinds.close();
}


static cv::Scalar getColor(int label) 
{
    return cv::Scalar(label * 2, 255 - label * 2, label + 50);
}


static cv::Mat process_result(cv::Mat& image, const vitis::ai::YOLOv6Result& result_in, bool print_result)
{
    /* Read the class labels from text file exist in same dirctory */
    const std::string labelsPath = "coco-labels-2014_2017.txt";
    std::vector<std::string> classes;
    LoadWords(labelsPath, classes);

    if (classes.size() == 0)
    {
        std::cerr << "\nError: No words exist in file " << labelsPath << std::endl;
        return cv::Mat();  // Return empty cv::Mat
    }

    /* Print results */
    for (const auto& result : result_in.bboxes)
    {
        int label = result.label;
        auto& box = result.box;
        LOG_IF(INFO, print_result) << "RESULT: " << classes[label] << "\t" << std::fixed
                                << std::setprecision(2) << box[0] << "\t" << box[1]
                                << "\t" << box[2] << "\t" << box[3] << "\t"
                                << std::setprecision(6) << result.score << "\n";

        cv::rectangle(image, cv::Point(box[0], box[1]), cv::Point(box[2], box[3]),
                        getColor(label), 1, 1, 0);
    }
    return image;
}