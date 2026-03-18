extern "C"
{
#include <libavformat/avformat.h>
    // Dummy just so we have a symbol to export.
    void casturria_dummy()
    {
        avformat_alloc_context();
    }
}