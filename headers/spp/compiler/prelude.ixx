module;
#include <spp/macros.hpp>

export module spp.compiler:prelude;
import std;

namespace spp::compiler {
    SPP_EXP_CMP const std::string PRELUDE = R"(
use std::annotations::public
use std::annotations::protected
use std::annotations::private
use std::annotations::virtual_method
use std::annotations::abstract_method
use std::annotations::ffi
use std::annotations::Annotation
use std::string::Str
use std::string_view::StrView
use std::boolean::Bool
use std::number::U8
use std::number::U16
use std::number::U32
use std::number::U64
use std::number::U128
use std::number::U256
use std::number::USize
use std::number::S8
use std::number::S16
use std::number::S32
use std::number::S64
use std::number::S128
use std::number::S256
use std::number::SSize
use std::number::F8
use std::number::F16
use std::number::F32
use std::number::F64
use std::number::F128
use std::void::Void
use std::option::Opt
use std::option::None
use std::option::Some
use std::result::Res
use std::result::Pass
use std::result::Fail
use std::array::Arr
use std::vector::Vec
use std::slice::Slice
use std::tuple::Tup
use std::single::Single
use std::function::FunRef
use std::function::FunMut
use std::function::FunMov
use std::cast::From
use std::generator::Gen
use std::generator::GenOnce
use std::copy::Copy
)";
}
