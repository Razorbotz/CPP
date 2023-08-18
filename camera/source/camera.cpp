
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <iostream>

#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <chrono>

#include <glib.h>
#include <gst/gst.h>




int main(int argc, char **argv){

    gst_init (&argc, &argv);

    std::string cameraPath="/dev/video0";
    std::string hostAddress="172.0.0.1";
    int port=33333;

    GMainLoop* loop = g_main_loop_new (NULL, FALSE);
    GstElement* pipeline = gst_pipeline_new("cameraPipeline");

    GstElement* v4l2src = gst_element_factory_make("v4l2src", "v4l2src");
    if (v4l2src == NULL) std::cout << "Could not create 'v4l2src' element" << std::endl;
    g_object_set(G_OBJECT(v4l2src), "device", cameraPath, NULL);
    GstElement* videoconvert = gst_element_factory_make("videoconvert","videoconvert");
    if (videoconvert == NULL) std::cout << "Could not create 'videoconvert' element" << std::endl;
    GstElement* videoscale = gst_element_factory_make("videoscale","videoscale");
    if (videoscale == NULL) std::cout << "Could not create 'videoscale' element" << std::endl;
    GstElement* capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    g_assert(capsfilter != NULL); /* should always exist */
    GstCaps* capsfilterParameters = gst_caps_new_simple("video/x-raw",
               "width", G_TYPE_INT, 320,
               "height", G_TYPE_INT, 240,
               NULL);
    g_object_set(G_OBJECT(capsfilter), "caps", capsfilterParameters, NULL);
    gst_caps_unref(capsfilterParameters);
    GstElement* theoraenc = gst_element_factory_make("theoraenc","theoraenc");
    if (theoraenc == NULL) std::cout << "Could not create 'theoraenc' element" << std::endl;
    GstElement* oggmux = gst_element_factory_make("oggmux","oggmux");
    if (oggmux == NULL) std::cout << "Could not create 'oggmux' element" << std::endl;
    GstElement* tcpserversink = gst_element_factory_make("tcpserversink","tcpserversink");
    if (tcpserversink == NULL) std::cout << "Could not create 'tcpserversink' element" << std::endl;
    g_object_set(G_OBJECT(tcpserversink), "host", hostAddress.c_str(), NULL);
    g_object_set(G_OBJECT(tcpserversink), "port", (gint)port, NULL);

    //gst_bin_add_many(GST_BIN(pipeline), v4l2src, videoconvert, videoscale, capsfilter, theoraenc, oggmux, tcpserversink, NULL);
    if(!gst_bin_add(GST_BIN(pipeline), v4l2src)) std::cerr << "failed to add v4l2src" << std::endl;
    if(!gst_bin_add(GST_BIN(pipeline), videoconvert)) std::cerr << "failed to add videoconvert" << std::endl;
    if(!gst_bin_add(GST_BIN(pipeline), videoscale)) std::cerr << "failed to add videoscale" << std::endl;
    //if(!gst_bin_add(GST_BIN(pipeline), capsfilter)) std::cerr << "failed to add capsfilter" << std::endl;
    if(!gst_bin_add(GST_BIN(pipeline), theoraenc)) std::cerr << "fail to add theoraenc" << std::endl;
    if(!gst_bin_add(GST_BIN(pipeline), oggmux)) std::cerr << "failed to add oggmux" << std::endl; 
    if(!gst_bin_add(GST_BIN(pipeline), tcpserversink)) std::cerr << "failed to add tcpserversink" << std::endl;

    if(!gst_element_link(v4l2src, videoconvert)) std::cerr << "failed to link v4l2src and videoconvert" << std::endl;
    if(!gst_element_link(videoconvert, videoscale)) std::cerr << "failed to link videoconvert and videoscale" << std::endl;
    if(!gst_element_link(videoscale, theoraenc)) std::cerr << "failed to link videoscale and capsfilter" << std::endl;
    //if(!gst_element_link(videoscale, capsfilter)) std::cerr << "failed to link videoscale and capsfilter" << std::endl;
    //if(!gst_element_link(capsfilter, theoraenc)) std::cerr << "failed to link capsfilter and theoraenc" << std::endl;
    if(!gst_element_link(theoraenc, oggmux)) std::cerr << "failed to link theoraenc and oggmux" << std::endl;
    if(!gst_element_link(oggmux, tcpserversink)) std::cerr << "failed to link oggmux and tcpserversink" << std::endl;

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    std::cout << "begining loop " << std::endl;
    g_main_loop_run (loop);
    while(true){
        while(g_main_context_pending(g_main_context_default())){
            g_main_context_iteration(g_main_context_default(), false);	
        }
    }

    g_main_loop_unref(loop);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));
    gst_object_unref(v4l2src);
    gst_object_unref(videoconvert);
    gst_object_unref(videoscale);
    gst_object_unref(theoraenc);
    gst_object_unref(oggmux);
    gst_object_unref(tcpserversink);
}
