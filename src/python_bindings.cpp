#include <boost/python.hpp>

#include "dta/dta.hpp"
#include "dta/json.hpp"

namespace {

std::string assess_json(const std::string& input) {
    try {
        return dta::assess_json(input);
    } catch (const std::exception& error) {
        PyErr_SetString(PyExc_ValueError, error.what());
        boost::python::throw_error_already_set();
    }
    return {};
}

} // namespace

BOOST_PYTHON_MODULE(dta) {
    boost::python::def("add", &dta::add);
    boost::python::def("assess_json", &assess_json);
}
