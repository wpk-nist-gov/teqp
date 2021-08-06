#include "pybind11_wrapper.hpp"

#include "teqp/models/multifluid.hpp"
#include "teqp/derivs.hpp"

void add_multifluid_mutant(py::module& m) {

    // A typedef for the base model
    using MultiFluid = decltype(build_multifluid_model(std::vector<std::string>{"", ""}, "", ""));
    
    // Wrap the function for generating a multifluid mutant
    m.def("build_multifluid_mutant", &build_multifluid_mutant<MultiFluid>);

    // The reducing function type and the departure function types are the same
    // as the base model
    using RedType = std::decay_t<decltype(MultiFluid::redfunc)>;
    using DepType = std::decay_t<decltype(MultiFluid::dep)>;

    // Typedef for modified mutant type
    using BIPmod = std::decay_t<MultiFluidAdapter<RedType, DepType, MultiFluid>>;

    // Define python wrapper of the mutant class
    auto wMFBIP = py::class_<BIPmod>(m, "MultiFluidMutant")
        .def("set_meta", [](BIPmod& c, const std::string &s) { return c.set_meta(s); })
        .def("get_meta", [](const BIPmod& c) { return c.get_meta(); })
        ;
    add_derivatives<BIPmod>(m, wMFBIP);
}