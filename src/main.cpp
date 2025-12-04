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


//======================================================================================================================
/// A simple assertion function + macro
inline void myAssert(bool b, const std::string &s = "MYASSERT ERROR !") {
    if (!b)
        throw std::runtime_error(s);
}

#define MY_ASSERT(x) myAssert(x, "MYASSERT ERROR :" #x)

//======================================================================================================================
/// Check GStreamer error, exit on error
inline void checkErr(GError *err) {
    if (err) {
        std::cerr << "checkErr : " << err->message << std::endl;
        exit(0);
    }
}

//======================================================================================================================
/// Our global data
struct GoblinData {
    // The two pipelines
    GstElement *goblinPipeline = nullptr;
    GstElement *goblinSinkV = nullptr;
    GstElement *elfPipeline = nullptr;
    GstElement *elfSrcV = nullptr;

    /// Appsrc flag: when it's true, send the frames, otherwise wait
    std::atomic_bool flagRunV{false};
    /// True if the elf pipeline has initialized and started splaying
    std::atomic_bool flagElfStarted{false};
};

//======================================================================================================================
/// Process a single bus message, log messages, exit on error, return false on eof
static bool busProcessMsg(GstElement *pipeline, GstMessage *msg, const std::string &prefix) {
    using namespace std;

    GstMessageType mType = GST_MESSAGE_TYPE(msg);
    // cout <<"[" << prefix << "]: ";
    switch (mType) {
        case (GST_MESSAGE_ERROR):
            // Parse error and exit program, hard exit
            GError *err;
            gchar *dbg;
            gst_message_parse_error(msg, &err, &dbg);
            cout << "ERR = " << err->message << " FROM " << GST_OBJECT_NAME(msg->src) << endl;
            cout << "DBG = " << dbg << endl;
            g_clear_error(&err);
            g_free(dbg);
            exit(1);
        case (GST_MESSAGE_EOS) :
            // Soft exit on EOS
            cout << " EOS !" << endl;
            return false;
        case (GST_MESSAGE_STATE_CHANGED):
            // Parse state change, print extra info for pipeline only
            cout << "State changed !" << endl;
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                GstState sOld, sNew, sPenging;
                gst_message_parse_state_changed(msg, &sOld, &sNew, &sPenging);
                cout << "Pipeline changed from " << gst_element_state_get_name(sOld) << " to " <<
                     gst_element_state_get_name(sNew) << endl;
            }
            break;
        case (GST_MESSAGE_STEP_START):
            cout << "STEP START !" << endl;
            break;
        case (GST_MESSAGE_STREAM_STATUS):
            cout << "STREAM STATUS !" << endl;
            break;
        case (GST_MESSAGE_ELEMENT):
            cout << "MESSAGE ELEMENT !" << endl;
            break;
        // You can add more stuff here if you want

        default:
            // cout << endl;
            break;
    }
    return true;
}

//======================================================================================================================
/// Run the message loop for one bus
void codeThreadBus(GstElement *pipeline, GoblinData &data, const std::string &prefix) {
    using namespace std;
    GstBus *bus = gst_element_get_bus(pipeline);

    int res;
    while (true) {
        GstMessage *msg = gst_bus_timed_pop(bus, GST_CLOCK_TIME_NONE);
        MY_ASSERT(msg);
        res = busProcessMsg(pipeline, msg, prefix);
        gst_message_unref(msg);
        if (!res)
            break;
    }
    gst_object_unref(bus);
    cout << "BUS THREAD FINISHED : " << prefix << endl;
}

//======================================================================================================================
/// Take frames from appsink, process with opencv, send to appsrc
void codeThreadProcessV(GoblinData &data) {
    using namespace std;
    using namespace cv;

    for (;;) {
        // We wait until ELF wants data, but only if ELF is already started
        while (data.flagElfStarted && !data.flagRunV) {
            cout << "(wait)" << endl;
            this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Check for Goblin EOS
        if (gst_app_sink_is_eos(GST_APP_SINK(data.goblinSinkV))) {
            cout << "GOBLIN EOS !" << endl;
            break;
        }

        // Pull the sample from Goblin appsink
        GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(data.goblinSinkV));
        if (sample == nullptr) {
            cout << "NO sample !" << endl;
            break;
        }

        // Get width and height from sample caps
        GstCaps *caps = gst_sample_get_caps(sample);
        myAssert(caps != nullptr);

        GstStructure *s = gst_caps_get_structure(caps, 0);
        int imW, imH;
        MY_ASSERT(gst_structure_get_int(s, "width", &imW));
        MY_ASSERT(gst_structure_get_int(s, "height", &imH));
        int f1, f2;
        MY_ASSERT(gst_structure_get_fraction(s, "framerate", &f1, &f2));
        // cout << "Sample: W = " << imW << ", H = " << imH << ", framerate = " << f1 << " / " << f2 << endl;

        // Check if ELF is initialized
        if (!data.flagElfStarted) {
            // // Use sample caps verbatim to ELF appsrc and re-negotiate
            // // Make a copy to be safe (probably not needed)
            // GstCaps *capsElf = gst_caps_copy(caps);
            // g_object_set(data.elfSrcV, "caps", capsElf, nullptr);
            // gst_caps_unref(capsElf);

            // Now we can play the ELF pipeline
            GstStateChangeReturn ret = gst_element_set_state(data.elfPipeline, GST_STATE_PLAYING);
            MY_ASSERT(ret != GST_STATE_CHANGE_FAILURE);
            data.flagElfStarted = true;
        }

        // Copy data from the sample to cv::Mat()
        GstBuffer *bufferIn = gst_sample_get_buffer(sample);
        GstMapInfo mapIn;
        myAssert(gst_buffer_map(bufferIn, &mapIn, GST_MAP_READ));
        myAssert(mapIn.size == imW * imH * 3);
        // Don't forget the Timestamp
        // uint64_t pts = bufferIn->pts;
        // uint64_t duration = bufferIn->duration;

        // Clone to be safe, we don't want to modify the input buffer
        Mat frame = Mat(imH, imW, CV_8UC3, (void *) mapIn.data).clone();
        gst_buffer_unmap(bufferIn, &mapIn);

        // Modify the frame: apply photo negative to the middle 1/9 of the image
        // Mat frameMid(frame, Rect2i(imW/3, imH/3, imW/3, imH/3));
        // bitwise_not(frameMid, frameMid);
        // Create the output bufer and send it to elfSrc
        int bufferSize = frame.cols * frame.rows * 3;
        GstBuffer *bufferOut = gst_buffer_new_and_alloc(bufferSize);
        GstMapInfo mapOut;
        gst_buffer_map(bufferOut, &mapOut, GST_MAP_WRITE);
        memcpy(mapOut.data, frame.data, bufferSize);
        gst_buffer_unmap(bufferOut, &mapOut);
        // Copy the input packet timestamp
        // bufferOut->pts = pts;
        // bufferOut->duration = duration;
        GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(data.elfSrcV), bufferOut);

        switch (ret)
        {
            case(GST_FLOW_OK):
                cout << "OK" << endl;
                break;
            case(GST_FLOW_FLUSHING):
                cout << "Flushing" << endl;
                break;
            case(GST_FLOW_EOS):
                cout << "EOS" << endl;
                break;
            default:
                cout << "This one I don't know huh" << endl;
                break;

        }

    }
    // Send EOS to ELF
    gst_app_src_end_of_stream(GST_APP_SRC(data.elfSrcV));
}
//======================================================================================================================
/// Callback called when the pipeline wants more data
static void startFeed(GstElement *source, guint size, GoblinData *data) {
    using namespace std;
    if (!data->flagRunV) {
        cout << "startFeed !" << endl;
        data->flagRunV = true;
    }
}

//======================================================================================================================
/// Callback called when the pipeline wants no more data for now
static void stopFeed(GstElement *source, GoblinData *data) {
    using namespace std;
    if (data->flagRunV) {
        cout << "stopFeed !" << endl;
        data->flagRunV = false;
    }
}

//======================================================================================================================
int main(int argc, char **argv){
    using namespace std;
    cout << "VIDEO3: Two pipelines, with custom video processing in the middle" << endl;

    // Init gstreamer
    gst_init(&argc, &argv);

    if (argc != 2) {
        cout << "Usage:\nvideo3 <video_file>" << endl;
        return 0;
    }
    string fileName(argv[1]);
    cout << "Playing file : " << fileName << endl;

    // Our global data
    GoblinData data;

    //Input pipeline
    string pipeStrGoblin = "filesrc location="+fileName+" !"
        " qtdemux name=videodemux videodemux.video_0 !"
        " h264parse ! omxh264dec ! video/x-raw,format=NV12,width=1920,height=1200,framerate=30/1 !"
        " videoconvert !"
        " video/x-raw,format=RGB !"
        " appsink name=goblin_sink sync=false max-buffers=32";

    // string pipeStrGoblin = "videotestsrc pattern=ball motion=1 flip=true ! video/x-raw,format=RGB,width=1920,height=1200,framerate=30/1 !"
    //     " appsink name=goblin_sink sync=false max-buffers=32";
    // Output pipeline
    string pipeStrElf = "appsrc name=elf_src do-timestamp=true format=time caps=video/x-raw,format=RGB,width=1920,height=1200,framerate=30/1 !"
                        " queue !"
                        " kmssink bus-id=fd4a0000.display sync=true";
    
    GError *err = nullptr;
    data.goblinPipeline = gst_parse_launch(pipeStrGoblin.c_str(), &err);
    checkErr(err);
    MY_ASSERT(data.goblinPipeline);
    data.goblinSinkV = gst_bin_get_by_name(GST_BIN (data.goblinPipeline), "goblin_sink");
    MY_ASSERT(data.goblinSinkV);

    // Set up ELF (output pipeline)
    // Note that appsrc does not have full caps yet as usual

    data.elfPipeline = gst_parse_launch(pipeStrElf.c_str(), &err);
    checkErr(err);
    MY_ASSERT(data.elfPipeline);
    data.elfSrcV = gst_bin_get_by_name(GST_BIN (data.elfPipeline), "elf_src");
    MY_ASSERT(data.elfSrcV);
    // Add calbacks like in video2
    g_signal_connect(data.elfSrcV, "need-data", G_CALLBACK(startFeed), &data);
    g_signal_connect(data.elfSrcV, "enough-data", G_CALLBACK(stopFeed), &data);


    // Play the Goblin pipeline only (Elf will start a bit later)
    MY_ASSERT(gst_element_set_state(data.goblinPipeline, GST_STATE_PLAYING));


    // Video processing thread (from goblin appsink to elf appsrc)
    thread threadProcessV([&data]{
        codeThreadProcessV(data);
    });
    // Now we need two bus threads: one for each pipeline !
    thread threadBusGoblin([&data]{
        codeThreadBus(data.goblinPipeline, data, "GOBLIN");
    });
    thread threadBusElf([&data]{
        codeThreadBus(data.elfPipeline, data, "ELF");
    });

    // Wait for threads
    threadProcessV.join();
    threadBusGoblin.join();
    threadBusElf.join();

    // Destroy the two pipelines
    gst_element_set_state(data.goblinPipeline, GST_STATE_NULL);
    gst_object_unref(data.goblinPipeline);
    gst_element_set_state(data.elfPipeline, GST_STATE_NULL);
    gst_object_unref(data.elfPipeline);

    return 0;
}
//======================================================================================================================