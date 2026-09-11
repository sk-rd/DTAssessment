#include <boost/python.hpp>

#include "dta/dta.hpp"

BOOST_PYTHON_MODULE(dta) {
    boost::python::def("add", &dta::add);
}
