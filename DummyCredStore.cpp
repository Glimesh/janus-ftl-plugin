#include "DummyCredStore.h"

std::string DummyCredStore::GetHmacKey(uint32_t userId)
{
    return "aBcDeFgHiJkLmNoPqRsTuVwXyZ123456";
}