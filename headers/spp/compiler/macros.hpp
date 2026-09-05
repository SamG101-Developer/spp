#pragma once

// Gate the code following this behind a feature. For example,
// memory stack protection is only applied if [memory.stack]
// contains "protect=true". Braces needed after the macro use.
#define FEATURE_GATE(_What) \
  if (utils::features::Enabled(utils::features::ConfigKey::_What))
