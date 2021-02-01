#include <gst/gst.h>
//#include <iostream>

#define DEFAULT_RTSP_LOCATION "rtsp://127.0.0.1:8554/frames"

static char *location = (char *) DEFAULT_RTSP_LOCATION;

static GOptionEntry entries[] = {
    {"location", 'l', 0, G_OPTION_ARG_STRING, &location,
     "Location of the RTSP url to read (default: " DEFAULT_RTSP_LOCATION ")", "LOCATION"},
    {NULL}
};
//
/// Structure which coontains gstreamer elements.
//
typedef struct _CustomData
{
    GstElement *pipeline;
    GstElement *rtspsrc;
    GstElement *rtph264depay;
    GstElement *decodebin;
    GstElement *videoconvert;
    GstElement *imagesink;
    GstElement *jpegenc;
    GstElement *videoenc;
    GstElement *muxer;
    GstElement *videosink;

} CustomData;

//
/// GLib's Main Loop.
//
GMainLoop *main_loop;

//
///  This function is called by the pad-added signal.
//
void pad_added_handler(GstElement *src, GstPad *new_pad, gpointer data)
{
    GstPad *sink_pad = NULL;
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;
    gchar *pad_name = GST_PAD_NAME(new_pad);

    g_print ("Received new pad '%s' from '%s':\n", pad_name, GST_ELEMENT_NAME(src));

    // Check the new pad's type.
    new_pad_caps = gst_pad_get_current_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);
    //if (g_str_has_prefix(new_pad_type, "video"))
    sink_pad = gst_element_get_static_pad((GstElement*)data, "sink");
    //TODO: skip audio for now.
    //else if(g_str_has_prefix(new_pad_type, "audio"))
    //    sink_pad = gst_element_get_static_pad((GstElement*)data, "sink");
    /*else
    {
        g_print("It has type '%s' which is not video/audio. Ignoring.\n", new_pad_type);
        // Unreference the new pad's caps, if we got them.
        if (new_pad_caps != NULL)
            gst_caps_unref(new_pad_caps);
        return;
    }*/

    // If it's already linked, we have nothing to do here.
    if (gst_pad_is_linked(sink_pad))
    {
        g_print("We are already linked. Ignoring.\n");
        // Unreference the sink pad.
        gst_object_unref(sink_pad);
        return;
    }

    // Attempt the link.
    ret = gst_pad_link(new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED(ret))
    {
        g_print("Type is '%s' but link failed.\n", new_pad_type);
        // Unreference the new pad's caps, if we got them.
        if (new_pad_caps != NULL)
            gst_caps_unref(new_pad_caps);
        // Unreference the sink pad.
        gst_object_unref(sink_pad);
    }
    else
    {
        g_print("Link succeeded (type '%s').\n", new_pad_type);
    }

} // pad_added_handler


//
/// Called when we get a GstMessage from the pipeline. When we get EOS,
/// we exit the mainloop.
//
gboolean bus_callback(GstBus * /*bus*/, GstMessage * msg, gpointer data)
{
    switch (GST_MESSAGE_TYPE(msg))
    {
        case GST_MESSAGE_EOS:
            g_print("End-Of-Stream reached.\n");
            g_main_loop_quit(main_loop);
            break;
        case GST_MESSAGE_ERROR:
            {
                GError *err;
                gchar *debug_info;
                gst_message_parse_error(msg, &err, &debug_info);
                g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
                g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
                g_clear_error(&err);
                g_main_loop_quit(main_loop);
            }
            break;
        case GST_MESSAGE_WARNING:
            {
                GError *err;
                gchar *debug;
                gst_message_parse_warning(msg, &err, &debug);
                g_print("Warning received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
                g_error_free(err);
                g_free(debug);
            }
            break;
        case GST_MESSAGE_STATE_CHANGED:
            // We are only interested in state-changed messages from the pipeline.
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT((GstElement*)data))
            {
                GstState old_state, new_state, pending_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                g_print("Pipeline state changed from %s to %s:\n",
                gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
            }
            break;
        default:
            {} // Do nothing.
    }
    // We want to keep receiving messages. FALSE means we want to stop watching
    // for messages on the bus and our callback should not be called again.
    return TRUE;
}

int main (int argc, char *argv[])
{
    GOptionContext *optctx;
	GError *error = NULL;

	optctx = g_option_context_new("<launch line> - Test RTSP Server, receiver\n");
	g_option_context_add_main_entries(optctx, entries, NULL);
	g_option_context_add_group(optctx, gst_init_get_option_group());
	if (!g_option_context_parse(optctx, &argc, &argv, &error))
	{
		g_printerr("Error parsing options: %s\n", error->message);
		g_option_context_free(optctx);
		g_clear_error(&error);
		return -1;
	}
	g_option_context_free(optctx);

	// Initialize GStreamer.
    gst_init(NULL, NULL);

    bool bSaveFrames = false;
    if (strstr(location, "frames"))
    {
        bSaveFrames = true;
        g_print("Stream frames are saved to frames directory\n");
    }
    else
        g_print("Stream is saved as mp4 video - out_video.mp4\n");
    //
    // Create gstreamer graph elements․
    //
    CustomData data;
    // Create an empty pipeline.
    data.pipeline = gst_pipeline_new("receiver_pipeline");
    data.rtspsrc = gst_element_factory_make("rtspsrc", "rtspsrc");
    data.rtph264depay = gst_element_factory_make("rtph264depay", "rtph264depay");
    data.decodebin = gst_element_factory_make("decodebin", "decodebin");
    data.videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
    data.jpegenc = gst_element_factory_make("jpegenc", "jpegenc");
    data.imagesink = gst_element_factory_make("multifilesink", "multifilesink");
    data.videoenc = gst_element_factory_make("x264enc", "x264enc");
    data.muxer = gst_element_factory_make("mp4mux", "muxer");
    data.videosink = gst_element_factory_make("filesink", "filesink");

    if (!data.pipeline || !data.rtspsrc || !data.rtph264depay || !data.decodebin || !data.videoconvert || !data.jpegenc
            || !data.imagesink || !data.videoenc || !data.muxer || !data.videosink)
    {
        g_printerr("Not all gstreamer elements could be created.\n");
        return -1;
    }

    //
    // Build the pipeline.
    //
    if (bSaveFrames)
    {
        gst_bin_add_many(GST_BIN(data.pipeline), data.rtspsrc, data.rtph264depay, data.decodebin,
                     data.videoconvert, data.jpegenc, data.imagesink, NULL);
    }
    else // save stream as mp4 file
    {
        gst_bin_add_many(GST_BIN(data.pipeline), data.rtspsrc, data.rtph264depay, data.decodebin,
                     data.videoconvert, data.videoenc, data.muxer, data.videosink, NULL);
    }
    //
    // Link elements.
    // Note that we are NOT linking rtspsrc to rtph264depay at this point(because it's a dynamic element).
    // Note that we are NOT linking decodebin with videoconvert at this point(because it's a dynamic element).
    // We will do it later.
    //
    if (!gst_element_link_many(data.rtph264depay, data.decodebin, NULL))
    {
        g_printerr("Elements could not be linked to decodebin.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    // Is element linking succeded?
    bool bLinkElements;

    if (bSaveFrames)
    {
        bLinkElements = gst_element_link_many(data.videoconvert, data.jpegenc, data.imagesink, NULL);
    }
    else
    {
        bLinkElements = gst_element_link_many(data.videoconvert, data.videoenc, data.muxer, data.videosink, NULL);
    }
    if (!bLinkElements)
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    //
    // Configure elements.
    //
    g_object_set(data.rtspsrc, "location", location, NULL); // Location of the RTSP url to read.
    g_object_set(data.imagesink, "location", "frames/img_%1d.jpg", NULL); // Location to save frames.
    g_object_set(data.videosink, "location", "out_video.mp4", NULL); // Location to save video.

    // Connect to the pad-added signal.
    g_signal_connect(data.rtspsrc, "pad-added", G_CALLBACK(pad_added_handler), data.rtph264depay);
    g_signal_connect(data.decodebin, "pad-added", G_CALLBACK(pad_added_handler), data.videoconvert);

    //
    // Start playing pipeline.
    //
    GstStateChangeReturn ret;
    ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    // To be notified of messages from this pipeline.
    GstBus *bus = NULL;
    bus = gst_element_get_bus(data.pipeline);
    gst_bus_add_watch(bus, (GstBusFunc)bus_callback, data.pipeline);
    gst_object_unref(bus);

    // Create a GLib Main Loop and set it to run.
    main_loop = g_main_loop_new(NULL, FALSE);
    // This loop will quit when the sink pipeline goes EOS or when an
    // error occurs in the source or sink pipelines.
    g_print("Let's run!\n");
    g_main_loop_run(main_loop);
    g_print("Going out\n");

    // Free resources.
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
    g_main_loop_unref(main_loop);

    return 0;
}