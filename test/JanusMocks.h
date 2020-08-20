#pragma once

extern "C"
{
    #include <debug.h>
    #include <rtp.h>
    #include <plugins/plugin.h>
}

// Define some janus logging stuff
gboolean janus_log_timestamps = true;
gboolean janus_log_colors = true;
int janus_log_level = LOG_NONE;
char *janus_log_global_prefix = NULL;

extern "C"
{
    gboolean janus_is_rtp(char *buf, guint len)
    {
        return true;
    }

    void janus_vprintf(const char *format, ...)
    { 
        // Do nothing
    }

    const char *janus_get_api_error(int error)
    {
        return "TEST: NOT IMPLEMENTED";
    }

    gint64 janus_get_real_time(void) {
        struct timespec ts;
        clock_gettime (CLOCK_REALTIME, &ts);
        return (ts.tv_sec*G_GINT64_CONSTANT(1000000)) + (ts.tv_nsec/G_GINT64_CONSTANT(1000));
    }

    void janus_rtp_switching_context_reset(janus_rtp_switching_context *context) {
        if(context == NULL)
        {
            return;
        }
        /* Reset the context values */
        memset(context, 0, sizeof(*context));
    }

    void janus_plugin_rtp_extensions_reset(janus_plugin_rtp_extensions *extensions) {
        if(extensions) {
            /* By extensions are not added to packets */
            extensions->audio_level = -1;
            extensions->audio_level_vad = FALSE;
            extensions->video_rotation = -1;
            extensions->video_back_camera = FALSE;
            extensions->video_flipped = FALSE;
        }
    }

    janus_plugin_result *janus_plugin_result_new(janus_plugin_result_type type, const char *text, json_t *content) {
        JANUS_LOG(LOG_HUGE, "Creating plugin result...\n");
        janus_plugin_result *result = static_cast<janus_plugin_result*>(g_malloc(sizeof(janus_plugin_result)));
        result->type = type;
        result->text = text;
        result->content = content;
        return result;
    }

    void janus_rtp_header_update(janus_rtp_header *header, janus_rtp_switching_context *context, gboolean video, int step)
    {
        // Do nothing
    }
}