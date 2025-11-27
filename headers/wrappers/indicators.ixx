module;
#include <indicators/progress_bar.hpp>

export module indicators;


export namespace indicators {
    using ::indicators::ProgressBar;

    namespace option {
        using ::indicators::option::BarWidth;
        using ::indicators::option::End;
        using ::indicators::option::Fill;
        using ::indicators::option::Lead;
        using ::indicators::option::MaxProgress;
        using ::indicators::option::PrefixText;
        using ::indicators::option::ShowElapsedTime;
        using ::indicators::option::ShowRemainingTime;
        using ::indicators::option::Start;
    }
}
