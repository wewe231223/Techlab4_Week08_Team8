#pragma once

#include "Containers/Array.h"
#include "ShadowCacheEntry.h"
#include "ShadowCasterReference.h"

bool CanReuseShadowMap(const FShadowCacheEntry& Current, const TArray<FShadowCasterReference>& CurrentCasters, const TArray<FShadowCasterReference>& RenderedCasters, const FShadowCacheEntry* Cached);
