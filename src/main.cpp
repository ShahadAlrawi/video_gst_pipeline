//
// Created by IT-JIM 
// VIDEO3: Two pipelines, with custom video processing in the middle, no audio

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <cmath>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

#include <opencv2/opencv.hpp>
#include <vitis/ai/yolov6.hpp>
#include "process_result.hpp"

//======================================================================================================================
/// A simple assertion function + macro
inline void myAssert(bool b, const std::string &s = "Assersion error")
{
    if (!b)
        throw std::runtime_error(s);
}

#define MY_ASSERT(x) myAssert(x, "Assersion error: " #x)



//======================================================================================================================
/// Check GStreamer error, exit on error
inline void checkErr(GError *err)
{
    if (err)
    {
        std::cerr << "checkErr : " << err->message << std::endl;
        exit(0);
    }
}



//======================================================================================================================
/// Our global data
struct GlobalData 
{
    // The two pipelines
    GstElement *inPipeline = nullptr;
    GstElement *inPipelineSrc = nullptr;
    GstElement *outPipeline = nullptr;
    GstElement *outPipelineSink = nullptr;

    /// when true, send the frames, otherwise wait
    std::atomic_bool runFlag{false};
    /// when true, either error or eos received from appsink
    std::atomic_bool endFlag{false};
    
};


struct FrameBuffer {
    cv::Mat frames[2];
    std::atomic<int> index{0};  // which frame is current
};


//======================================================================================================================
/// Process a single bus message, log messages, exit on error, return false on eof
static bool busProcessMsg(GstElement *pipeline, GstMessage *msg, const std::string &prefix)
{
    GstMessageType mType = GST_MESSAGE_TYPE(msg);
    switch (mType)
    {
        case (GST_MESSAGE_ERROR):
            std::cout << "[" << prefix << "]";
            /*
                Parse error and exit program (hard exit)
            */
            GError *err;
            gchar *dbg;
            gst_message_parse_error(msg, &err, &dbg);
            std::cout << "ERR = " << err->message << " FROM " << GST_OBJECT_NAME(msg->src) << std::endl;
            std::cout << "DBG = " << dbg << std::endl;
            g_clear_error(&err);
            g_free(dbg);
            exit(1);
        case (GST_MESSAGE_EOS):
            /*
                end-of-stream (soft exit)
            */
            std::cout << "[" << prefix << "]: ";
            std::cout << "EOS" << std::endl;
            return false;
        case (GST_MESSAGE_STATE_CHANGED):
            /*
                Parse state change, print extra info for pipeline only
            */
            std::cout << "[" << prefix << "]: ";
            std::cout << "State changed" << std::endl;

            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                GstState sOld, sNew, sPenging;
                gst_message_parse_state_changed(msg, &sOld, &sNew, &sPenging);
                std::cout << "Pipeline changed from " << gst_element_state_get_name(sOld) << " to " << gst_element_state_get_name(sNew) << std::endl;
            }
            break;
        default:
            break;
    }
    return true;
}



//======================================================================================================================
/// Run the message loop for one bus
void processMsg(GstElement *pipeline, GlobalData &data, const std::string &prefix)
{
    GstBus *bus = gst_element_get_bus(pipeline);

    int res;
    while (true) {
        GstMessage *msg = gst_bus_timed_pop(bus, GST_CLOCK_TIME_NONE);
        res = busProcessMsg(pipeline, msg, prefix);
        gst_message_unref(msg);
        if (!res)
            break;
    }
    gst_object_unref(bus);
    std::cout << "Bus thread finished: " << prefix << std::endl;
}



void runYolo(const std::string &model, GlobalData &data,  FrameBuffer &fb)
{
    auto yolo = vitis::ai::YOLOv6::create(model, true);

    if (!yolo)
    {   // supress coverity complain
        std::cout <<"create error"  << std::endl;
        exit(1);
    } 

    for (;;)
    {
        ///////////////////////////////////////////////////////////////////////
        // Find which index is next
        int current = fb.index.load();

        // Run yolo on the current frame
        auto result = yolo->run(fb.frames[current]);
        ///////////////////////////////////////////////////////////////////////

        if (data.endFlag)
        {
            break;
        }

    }
}





//======================================================================================================================
/// Take frames from appsink, process with opencv, send to appsrc
void processFrames(GlobalData &data, FrameBuffer &fb)
{
    for (;;)
    {
        // Wait until output pipeline needs data
        while (!data.runFlag) {
            std::cout << "(wait)" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        // Check for input stream flag: end-of-stream
        if (gst_app_sink_is_eos(GST_APP_SINK(data.inPipelineSrc))) {
            std::cout << "Recieved eos from input p" << std::endl;
            break;
        }

        // Pull the input pipeline
        GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(data.inPipelineSrc));
        if (sample == nullptr) {
            std::cout << "No frame" << std::endl;
            break;
        }

        // Copy data from the sample to cv::Mat()
        int imH = 1080;
        int imW = 1920;
        GstBuffer *bufferIn = gst_sample_get_buffer(sample);
        GstMapInfo mapIn;
        myAssert(gst_buffer_map(bufferIn, &mapIn, GST_MAP_READ));
        myAssert(mapIn.size == imW * imH * 3);
        // uint64_t pts = bufferIn->pts;
        // uint64_t duration = bufferIn->duration;

        // Clone to be safe, we don't want to modify the input buffer
        cv::Mat frame = cv::Mat(imH, imW, CV_8UC3, (void *) mapIn.data).clone();
        gst_buffer_unmap(bufferIn, &mapIn);
        gst_sample_unref(sample);

        ///////////////////////////////////////////////////////////////////////
        // Find which index is next
        int next = 1 - fb.index.load();

        // Copy into inactive buffer
        frame.copyTo(fb.frames[next]);

        // Publish frame (atomic flip)
        fb.index.store(next);
        ///////////////////////////////////////////////////////////////////////


        // Create the output bufer and send it to output pipeline
        int bufferSize = frame.cols * frame.rows * 3;
        GstBuffer *bufferOut = gst_buffer_new_and_alloc(bufferSize);
        GstMapInfo mapOut;
        gst_buffer_map(bufferOut, &mapOut, GST_MAP_WRITE);
        memcpy(mapOut.data, frame.data, bufferSize);
        gst_buffer_unmap(bufferOut, &mapOut);

        // Copy the input packet timestamp
        // bufferOut->pts = pts;
        // bufferOut->duration = duration;


        GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(data.outPipelineSink), bufferOut);
        if (ret != GST_FLOW_OK)
        {
            gst_buffer_unref(bufferOut);
        }
    }
    // Send EOS to output pipeline
    gst_app_src_end_of_stream(GST_APP_SRC(data.outPipelineSink));
    data.endFlag = true;
}



//======================================================================================================================
/// Callback called when the pipeline wants more data
static void startFeed(GstElement *source, guint size, GlobalData *data)
{
    using namespace std;
    if (!data->runFlag) {
        cout << "Need data" << endl;
        data->runFlag = true;
    }
}



//======================================================================================================================
/// Callback called when the pipeline wants no more data for now
static void stopFeed(GstElement *source, GlobalData *data)
{
    using namespace std;
    if (data->runFlag)
    {
        cout << "Enough data" << endl;
        data->runFlag = false;
    }
}



//======================================================================================================================
int main(int argc, char **argv)
{

    if (argc != 2)
    {
        std::cout << "Usage: "<< argv[0] << " <model_path>" << std::endl;
        exit(1);
    }
    std::string model = argv[1];

    // Init gstreamer
    gst_init(&argc, &argv);

    // Our global data
    GlobalData data;
    FrameBuffer fb;

    // Input pipeline
    std::string inPipeline = "v4l2src device=/dev/video0 io-mode=4 ! video/x-raw,width=1920,height=1080,format=RGB,framerate=30/1 !"
                        "appsink name=input_src sync=true max-buffers=2";



    // Output pipeline
    std::string outPipeline = "appsrc name=output_sink do-timestamp=true format=time caps=video/x-raw,format=RGB,width=1920,height=1080,framerate=30/1 !"
                        " queue !"
                        " kmssink bus-id=fd4a0000.display sync=true fullscreen-overlay=true";
    
    
    
    GError *err = nullptr;
    data.inPipeline = gst_parse_launch(inPipeline.c_str(), &err);
    checkErr(err);
    data.inPipelineSrc = gst_bin_get_by_name(GST_BIN(data.inPipeline), "input_src");


    data.outPipeline = gst_parse_launch(outPipeline.c_str(), &err);
    checkErr(err);
    data.outPipelineSink = gst_bin_get_by_name(GST_BIN(data.outPipeline), "output_sink");

    // Add calbacks
    g_signal_connect(data.outPipelineSink, "need-data", G_CALLBACK(startFeed), &data);
    g_signal_connect(data.outPipelineSink, "enough-data", G_CALLBACK(stopFeed), &data);

    // Play the input and output pipeline only
    gst_element_set_state(data.inPipeline, GST_STATE_PLAYING);
    gst_element_set_state(data.outPipeline, GST_STATE_PLAYING);

    // Video processing thread
    std::thread ThreadProcessFrames([&data, &fb]{
        processFrames(data, fb);
    });
    // Yolo thread
    std::thread ThreadRunYolo([&model, &data, &fb]{
        runYolo(model, data, fb);
    });
    // Now we need two bus threads: one for each pipeline
    std::thread ThreadprocessMsgIn([&data]{
        processMsg(data.inPipeline, data, "Input pipeline");
    });
    std::thread ThreadprocessMsgOut([&data]{
        processMsg(data.outPipeline, data, "Output pipeline");
    });

    // Wait for threads
    ThreadProcessFrames.join();
    ThreadRunYolo.join();
    ThreadprocessMsgIn.join();
    ThreadprocessMsgOut.join();

    // Destroy the two pipelines
    gst_element_set_state(data.inPipeline, GST_STATE_NULL);
    gst_object_unref(data.inPipeline);
    gst_element_set_state(data.outPipeline, GST_STATE_NULL);
    gst_object_unref(data.outPipeline);

    return 0;
}
//======================================================================================================================