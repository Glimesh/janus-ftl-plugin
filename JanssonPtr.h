/**
 * Lets us RAII-ify the json pointers returned by Jansson.
 * Thanks @RSATom!
 * https://github.com/RSATom/CxxPtr
 * Covered by GPL3.0 license
 */

#pragma once

#include <memory>

#include <jansson.h>


struct JanssonUnref
{
    void operator() (json_t* json)
        { json_decref(json); }
};

typedef
    std::unique_ptr<
        json_t,
        JanssonUnref> JsonPtr;