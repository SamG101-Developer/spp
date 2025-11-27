module;
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/process/v1.hpp>


export module boost;


export namespace boost::multiprecision {
    using boost::multiprecision::cpp_dec_float_100;
    using boost::multiprecision::cpp_int;

    using boost::process::v1::system;
}
