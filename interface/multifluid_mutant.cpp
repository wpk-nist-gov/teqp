#include "pybind11_wrapper.hpp"

#include "teqp/models/multifluid.hpp"
#include "teqp/derivs.hpp"

void add_multifluid_mutant(py::module& m) {

    using MultiFluid = decltype(build_multifluid_model(std::vector<std::string>{"", ""}, "", ""));
    m.def("build_BIPmodified", &build_BIPmodified<MultiFluid>);
    using RedType = std::decay_t<decltype(MultiFluid::redfunc)>; 
    using BIPmod = MultiFluidReducingFunctionAdapter<RedType, MultiFluid>;
    auto wMFBIP = py::class_<BIPmod>(m, "MultiFluidBIP");
    add_derivatives<BIPmod>(m, wMFBIP);
}