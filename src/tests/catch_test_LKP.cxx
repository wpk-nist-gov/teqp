#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using Catch::Approx;
#include <catch2/benchmark/catch_benchmark_all.hpp>

#include "teqp/derivs.hpp"
#include "teqp/constants.hpp"
#include "teqp/models/LKP.hpp"
#include "teqp/cpp/teqpcpp.hpp"

TEST_CASE("Check LKP", "[LKP]"){
    using namespace teqp::LKP;
    
    SECTION("pure"){
        // methane, check values from TREND
        std::vector<double> Tc_K = {190.564};
        std::vector<double> pc_Pa = {4.5992e6};
        std::vector<double> acentric = {0.011};
        std::vector<std::vector<double>> kmat{{1.0}};
        double R = 8.3144598;
        auto model = LKPMix(Tc_K, pc_Pa, acentric, R, kmat);
        auto z = (Eigen::ArrayXd(1) << 1.0).finished();
        
//        teqp::TDXDerivatives<decltype(model)>::get_Ar00(model, 300.0, 8000.0, z);
        
        nlohmann::json modelspec{
            {"Tcrit / K", Tc_K},
            {"pcrit / Pa", pc_Pa},
            {"acentric", acentric},
            {"R / J/mol/K", R},
            {"kmat", kmat}
        };
//        std::cout << spec.dump(2) << std::endl;
        
        CHECK_NOTHROW(make_LKPMix(modelspec));
        nlohmann::json badspec = modelspec;
        badspec["kmat"] = 4.7;
        CHECK_THROWS(make_LKPMix(badspec));
        
        nlohmann::json spec{
            {"kind", "LKP"},
            {"model", modelspec}
        };
        CHECK_NOTHROW(teqp::cppinterface::make_model(spec));
        auto ptr = teqp::cppinterface::make_model(spec);
        
        struct Point{ double T, rhomolar, alphar_expected; };
        for (auto& pt : std::vector<Point>{
            {2.5555555555556E+02,1.7778555555556E+03,-1.0483673775469E-01},
            {2.5555555555556E+02,2.6667333333333E+03,-1.5420416099894E-01},
            {2.5555555555556E+02,3.5556111111111E+03,-2.0148143793072E-01},
            {2.5555555555556E+02,4.4444888888889E+03,-2.4660690326310E-01},
            {2.5555555555556E+02,5.3333666666667E+03,-2.8951201335592E-01},
            {2.5555555555556E+02,6.2222444444444E+03,-3.3012829826090E-01},
            {2.5555555555556E+02,7.1111222222222E+03,-3.6839324530079E-01},
            {2.5555555555556E+02,8.0000000000000E+03,-4.0425381376141E-01},
            {2.7777777777778E+02,1.0000000000000E-01,-5.0166505188877E-06},
            {2.7777777777778E+02,8.8897777777778E+02,-4.3666195153684E-02},
            {2.7777777777778E+02,1.7778555555556E+03,-8.5458261689186E-02},
            {2.7777777777778E+02,2.6667333333333E+03,-1.2535703634585E-01},
            {2.7777777777778E+02,3.5556111111111E+03,-1.6332285849984E-01},
            {2.7777777777778E+02,4.4444888888889E+03,-1.9930364880452E-01},
            {2.7777777777778E+02,5.3333666666667E+03,-2.3323937266306E-01},
            {2.7777777777778E+02,6.2222444444444E+03,-2.6506680393110E-01},
            {2.7777777777778E+02,7.1111222222222E+03,-2.9472345661764E-01},
            {2.7777777777778E+02,8.0000000000000E+03,-3.2214967196215E-01},
            {3.0000000000000E+02,1.0000000000000E-01,-4.1178433147697E-06},
            {3.0000000000000E+02,8.8897777777778E+02,-3.5720133653505E-02},
            {3.0000000000000E+02,1.7778555555556E+03,-6.9656790541858E-02},
            {3.0000000000000E+02,2.6667333333333E+03,-1.0179417406048E-01},
            {3.0000000000000E+02,3.5556111111111E+03,-1.3209892931558E-01},
            {3.0000000000000E+02,4.4444888888889E+03,-1.6052611602765E-01},
            {3.0000000000000E+02,5.3333666666667E+03,-1.8702217613376E-01},
            {3.0000000000000E+02,6.2222444444444E+03,-2.1152813972213E-01},
            {3.0000000000000E+02,7.1111222222222E+03,-2.3398217049700E-01},
            {3.0000000000000E+02,8.0000000000000E+03,-2.5432064692151E-01}
        }){
            auto alphar_actual = teqp::TDXDerivatives<decltype(model)>::get_Ar00(model, pt.T, pt.rhomolar, z);
            CHECK(alphar_actual == Approx(pt.alphar_expected).margin(1e-12));
            
            auto alphar_actual_ptr = ptr->get_Ar00(pt.T, pt.rhomolar, z);
            CHECK(alphar_actual_ptr == Approx(pt.alphar_expected).margin(1e-12));
        }
    }
    
    SECTION("methane + nitrogen mix"){
        // methane + nitrogen, check values from TREND
        std::vector<double> Tc_K = {190.564, 126.192};
        std::vector<double> pc_Pa = {4.5992e6, 3.3958e6};
        std::vector<double> acentric = {0.011, 0.037};
        std::vector<std::vector<double>> kmat{{1.0, 0.977},{0.977, 1.0}};
        double R = 8.3144598;
        auto model = LKPMix(Tc_K, pc_Pa, acentric, R, kmat);
        auto z = (Eigen::ArrayXd(2) << 0.8, 0.2).finished();
        
        auto zbad = (Eigen::ArrayXd(3) << 0.3, 0.3, 0.4).finished();
        CHECK_THROWS(model.alphar(300.0, 8000.0, zbad));
        
        struct Point{ double T, rhomolar, alphar_expected; };
        for (auto& pt : std::vector<Point>{
            {2.55555555555555E+02, 1.00000000000000E-01, -4.91536626760729E-06},
            {2.55555555555555E+02, 8.88977777777777E+02, -4.28036717751216E-02},
            {2.55555555555555E+02, 1.77785555555555E+03, -8.38097840422122E-02},
            {2.55555555555555E+02, 2.66673333333333E+03, -1.23001289158271E-01},
            {2.55555555555555E+02, 3.55561111111111E+03, -1.60342053977241E-01},
            {2.55555555555555E+02, 4.44448888888888E+03, -1.95784417210887E-01},
            {2.55555555555555E+02, 5.33336666666666E+03, -2.29273119192599E-01},
            {2.55555555555555E+02, 6.22224444444444E+03, -2.60749579654642E-01},
            {2.55555555555555E+02, 7.11112222222222E+03, -2.90155546674414E-01},
            {2.55555555555555E+02, 8.00000000000000E+03, -3.17435227813972E-01},
            {2.77777777777777E+02, 1.00000000000000E-01, -3.96079440378770E-06},
            {2.77777777777777E+02, 8.88977777777777E+02, -3.43639521841312E-02},
            {2.77777777777777E+02, 1.77785555555555E+03, -6.70251681046093E-02},
            {2.77777777777777E+02, 2.66673333333333E+03, -9.79698096970028E-02},
            {2.77777777777777E+02, 3.55561111111111E+03, -1.27167929631439E-01},
            {2.77777777777777E+02, 4.44448888888888E+03, -1.54578932250068E-01},
            {2.77777777777777E+02, 5.33336666666666E+03, -1.80154076480535E-01},
            {2.77777777777777E+02, 6.22224444444444E+03, -2.03839249743354E-01},
            {2.77777777777777E+02, 7.11112222222222E+03, -2.25577252196130E-01},
            {2.77777777777777E+02, 8.00000000000000E+03, -2.45308899079892E-01},
            {3.00000000000000E+02, 1.00000000000000E-01, -3.18316290879055E-06},
            {3.00000000000000E+02, 8.88977777777777E+02, -2.74782561342433E-02},
            {3.00000000000000E+02, 1.77785555555555E+03, -5.33101587343266E-02},
            {3.00000000000000E+02, 2.66673333333333E+03, -7.74841431899544E-02},
            {3.00000000000000E+02, 3.55561111111111E+03, -9.99748284135246E-02},
            {3.00000000000000E+02, 4.44448888888888E+03, -1.20746896159584E-01},
            {3.00000000000000E+02, 5.33336666666666E+03, -1.39756583212869E-01},
            {3.00000000000000E+02, 6.22224444444444E+03, -1.56953388700025E-01},
            {3.00000000000000E+02, 7.11112222222222E+03, -1.72281392690949E-01},
            {3.00000000000000E+02, 8.00000000000000E+03, -1.85679636571464E-01}
        }){
            auto alphar_actual = teqp::TDXDerivatives<decltype(model)>::get_Ar00(model, pt.T, pt.rhomolar, z);
            CHECK(alphar_actual == Approx(pt.alphar_expected).margin(1e-12));
        }
    }
    
    // methane + nitrogen, check values from TREND
    std::vector<double> Tc_K = {190.564, 126.192};
    std::vector<double> pc_Pa = {4.5992e6, 3.3958e6};
    std::vector<double> acentric = {0.011, 0.037};
    std::vector<std::vector<double>> kmat{{1.0, 0.977},{0.977, 1.0}};
    double R = 8.3144598;
    auto model = LKPMix(Tc_K, pc_Pa, acentric, R, kmat);
    auto z = (Eigen::ArrayXd(2) << 0.8, 0.2).finished();
    
    nlohmann::json modelspec{
        {"Tcrit / K", Tc_K},
        {"pcrit / Pa", pc_Pa},
        {"acentric", acentric},
        {"R / J/mol/K", R},
        {"kmat", kmat}
    };
    nlohmann::json spec{
        {"kind", "LKP"},
        {"model", modelspec}
    };
    auto ptr = teqp::cppinterface::make_model(spec);
    BENCHMARK("evaluation"){
        return ptr->get_Ar00(300.0, 8000.0, z);
    };
}
