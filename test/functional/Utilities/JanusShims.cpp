/**
 * @file JanusShims.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2021-04-29
 * @copyright Copyright (c) 2021 Hayden McAfee
 */

extern "C"
{
    #include <glib.h>
    #include <plugins/plugin.h>
    #include <time.h>

    janus_plugin_result* janus_plugin_result_new(janus_plugin_result_type type, const char *text,
        json_t *content)
    {
        return nullptr;
    }

    gint64 janus_get_real_time(void)
    {
        struct timespec ts;
        clock_gettime (CLOCK_REALTIME, &ts);
        return (ts.tv_sec*G_GINT64_CONSTANT(1000000)) + (ts.tv_nsec/G_GINT64_CONSTANT(1000));
    }

    void janus_plugin_rtp_extensions_reset(janus_plugin_rtp_extensions *extensions)
    {
        // TODO: Excise use of this from our codebase...
    }
}