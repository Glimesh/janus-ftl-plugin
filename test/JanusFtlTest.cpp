#include <criterion/criterion.h>

#include "JanusMocks.h"
#include "JanusFtl.h"

#include <memory>

Test(janusftl, instantiate)
{
    std::unique_ptr<JanusFtl> janusFtl = std::make_unique<JanusFtl>(nullptr);
}