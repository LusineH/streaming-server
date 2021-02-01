#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#define DEFAULT_RTSP_PORT "8554"
#define DEFAULT_RTSP_MOUNT "/frames"

static char *port = (char *) DEFAULT_RTSP_PORT;
static char *mount = (char *) DEFAULT_RTSP_MOUNT;

static GOptionEntry entries[] = {
	{"port", 'p', 0, G_OPTION_ARG_STRING, &port,
		"Port to listen on (default: " DEFAULT_RTSP_PORT ")", "PORT"},
  {"mount", 'm', 0, G_OPTION_ARG_STRING, &mount,
		"RTSP mount point (default: " DEFAULT_RTSP_MOUNT ")", "MOUNT"},
	{NULL}
};

int main (int argc, char *argv[])
{
	GMainLoop *loop;
	GstRTSPServer *server;
	GstRTSPMountPoints *mounts;
	GstRTSPMediaFactory *factory;
	GOptionContext *optctx;
	GError *error = NULL;

	optctx = g_option_context_new("<launch line> - Test RTSP Server, Launch\n\n"
	  "Example: \"( videotestsrc num-buffers=300 ! x264enc ! rtph264pay name=pay0 )\"");
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

	loop = g_main_loop_new(NULL, FALSE);

	// Create a server instance.
	server = gst_rtsp_server_new();
	g_object_set(server, "service", port, NULL);

	// Get the mount points for this server, every server has a default object
	// that be used to map uri mount points to media factories.
	mounts = gst_rtsp_server_get_mount_points(server);

	// Make a media factory for a test stream. The default media factory can use
	// gst-launch syntax to create pipelines.
	// Any launch line works as long as it contains elements named pay%d. Each
	// element with pay%d names will be a stream.
	factory = gst_rtsp_media_factory_new();
	gst_rtsp_media_factory_set_launch(factory, "( videotestsrc num-buffers=300 ! x264enc ! rtph264pay name=pay0 )"/*argv[1]*/);
	//gst_rtsp_media_factory_set_shared(factory, TRUE);

	// Attach the test factory to the /mount url.
	gst_rtsp_mount_points_add_factory(mounts, mount, factory);

	// Allow user to access this resource.
	gst_rtsp_media_factory_add_role(factory, "user",
		GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
		GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, TRUE, NULL);

	// Don't need the ref to the mapper anymore.
	g_object_unref(mounts);

	// Make a new authentication manager.
	GstRTSPAuth *auth = gst_rtsp_auth_new();

	// Make default token, it has the same permissions as user.
	//GstRTSPToken *token = gst_rtsp_token_new(GST_RTSP_TOKEN_MEDIA_FACTORY_ROLE, G_TYPE_STRING, "user", NULL);
	//gst_rtsp_auth_set_default_token(auth, token);
	//gst_rtsp_token_unref(token);

	// Make user token.
	GstRTSPToken *token = gst_rtsp_token_new(GST_RTSP_TOKEN_MEDIA_FACTORY_ROLE, G_TYPE_STRING, "user", NULL);
	gchar* basic = gst_rtsp_auth_make_basic("user", "password");
	gst_rtsp_auth_add_basic(auth, basic, token);
	g_free(basic);
	gst_rtsp_token_unref(token);

	// Set as the server authentication manager.
	gst_rtsp_server_set_auth(server, auth);
	g_object_unref(auth);

	// Attach the server to the default maincontext.
	gst_rtsp_server_attach(server, NULL);

	// Start streaming.
	g_print("Stream ready at rtsp://127.0.0.1:%s%s\n", port, mount);
	g_main_loop_run(loop);

	return 0;
}
