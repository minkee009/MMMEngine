#include "MUID.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"


RTTR_REGISTRATION
{
    using namespace rttr;
    using namespace MMMEngine;

    registration::class_<Utility::MUID>("MUID")
        .method("ToString", &MMMEngine::Utility::MUID::ToString);
}
