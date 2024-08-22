#pragma once

#include <optional>
#include "teqp/exceptions.hpp"
#include "teqp/cpp/teqpcpp.hpp"

#include <Eigen/Dense>

using namespace teqp::cppinterface;

namespace teqp::algorithms::phase_equil{

struct SpecificationSidecar{
    std::size_t Nphases, Ncomponents, Nindependent;
    double* ptr_p_phase0;
    double* ptr_dpdT_phase0;
    Eigen::ArrayXd* ptr_dpdrho_phase0;
};

struct AbstractSpecification{
    virtual std::tuple<double, Eigen::ArrayXd> r_Jacobian(const Eigen::ArrayXd& x, const SpecificationSidecar& sidecar) const = 0;
    virtual ~AbstractSpecification() = default;
};
/***
 \brief Specification equation for temperature
 */
struct TSpecification : public AbstractSpecification{
private:
    const double m_Tspec;
public:
    TSpecification(double T) : m_Tspec(T) {};
    
    virtual std::tuple<double, Eigen::ArrayXd> r_Jacobian(const Eigen::ArrayXd& x, const SpecificationSidecar& sidecar) const override {
        double r = x[0] - m_Tspec;
        Eigen::ArrayXd J(x.size()); J.setZero();
        J(0) = 1;
        return std::make_tuple(r, J);
    };
};

/**
 \brief Specification of molar phase fraction in given phase
*/
struct BetaSpecification : public AbstractSpecification{
private:
    const double m_betaspec;
    const std::size_t m_iphase;
public:
    BetaSpecification(double beta, std::size_t iphase) : m_betaspec(beta), m_iphase(iphase) {};
    
    virtual std::tuple<double, Eigen::ArrayXd> r_Jacobian(const Eigen::ArrayXd& x, const SpecificationSidecar& sidecar) const override {
        double r = x[x.size()-sidecar.Nphases+m_iphase] - m_betaspec;
        Eigen::ArrayXd Jrow(x.size()); Jrow.setZero();
        Jrow(x.size()-sidecar.Nphases+m_iphase) = 1;
        return std::make_tuple(r, Jrow);
    };
};

/**
\brief Specification equation for pressure
 
 For the first phase, but which phase you pick doesn't matter since all the phases must have the same pressure since they are at mechanical equilibrium
 */
struct PSpecification : public AbstractSpecification{
private:
    const double m_p;
public:
    PSpecification(double p_Pa) : m_p(p_Pa) {};
    
    virtual std::tuple<double, Eigen::ArrayXd> r_Jacobian(const Eigen::ArrayXd& x, const SpecificationSidecar& sidecar) const override {
        double r = *sidecar.ptr_p_phase0 - m_p;
        Eigen::ArrayXd Jrow(x.size()); Jrow.setZero();
        
        Jrow(0) = *sidecar.ptr_dpdT_phase0;
        Jrow.segment(1, sidecar.Ncomponents) = *sidecar.ptr_dpdrho_phase0;
        return std::make_tuple(r, Jrow);
    };
};

/**
 \brief Specification equation for molar volume
 */
struct MolarVolumeSpecification : public AbstractSpecification{
private:
    const double m_vspec_m3mol;
public:
    MolarVolumeSpecification(double v_m3mol) : m_vspec_m3mol(v_m3mol) {};
    
    virtual std::tuple<double, Eigen::ArrayXd> r_Jacobian(const Eigen::ArrayXd& x, const SpecificationSidecar& sidecar) const override {
        double T = x[0];
        std::vector<Eigen::Map<const Eigen::ArrayXd>> rhovecs;
        std::vector<double> rho_phase;
        for (auto iphase_ = 0; iphase_ < sidecar.Nphases; ++iphase_){
            rhovecs.push_back(Eigen::Map<const Eigen::ArrayXd>(&x[1 + iphase_*sidecar.Ncomponents], sidecar.Ncomponents));
            rho_phase.push_back(rhovecs.back().sum());
        }
        const Eigen::Map<const Eigen::ArrayXd> betas(&x[x.size()-sidecar.Nphases], sidecar.Nphases);
        
        Eigen::ArrayXd Jrow(x.size()); Jrow.setZero();
        double v = 0.0;
        for (auto iphase = 0; iphase < sidecar.Nphases; ++iphase){
            v += betas[iphase]/rho_phase[iphase];
            Jrow(x.size()-sidecar.Nphases+iphase) = 1/rho_phase[iphase];
            Jrow.segment(1+iphase*sidecar.Ncomponents, sidecar.Ncomponents) = -betas[iphase]/rho_phase[iphase]/rho_phase[iphase];
        }
        double r = v - m_vspec_m3mol;
        return std::make_tuple(r, Jrow);
    };
};


/**
 
 */
class GeneralizedPhaseEquilibrium{
private:
    auto get_Ncomponents(const std::vector<Eigen::ArrayXd>& rhovecs){
        // Check all are the same size, and nonempty
        std::set<std::size_t> sizes;
        for (const auto& rhovec : rhovecs){
            sizes.emplace(rhovec.size());
        }
        if (sizes.size() != 1){
            throw;
        }
        for (auto size : sizes){
            if (size == 0){
                throw;
            }
            return size;
        }
        throw;
    }
public:
    
    struct CallResult{
        Eigen::VectorXd r;
        Eigen::MatrixXd J;
    };
    
    struct UnpackedVariables{
    public:
        const double T;
        const std::vector<Eigen::ArrayXd> rhovecs;
        const Eigen::ArrayXd betas;
        UnpackedVariables(const double T, const std::vector<Eigen::ArrayXd>& rhovecs, const Eigen::ArrayXd& betas) : T(T), rhovecs(rhovecs), betas(betas){};
        
        auto pack(){
            auto Nphases = betas.size();
            auto Ncomponents = rhovecs[0].size();
            Eigen::ArrayXd x(1+(Ncomponents+1)*Nphases);
            x[0] = T;
            for (auto iphase_ = 0; iphase_ < Nphases; ++iphase_){
                Eigen::Map<Eigen::ArrayXd>(&x[1 + iphase_*Ncomponents], Ncomponents) = rhovecs[iphase_];
            }
            x.tail(betas.size()) = betas;
            return x;
        }
    };
    const AbstractModel& residptr; ///< The pointer for the residual portion of \f$\alpha\f$
//    std::optional<std::unique_ptr<AbstractModel>&> idealgasptr; ///< The pointer for the ideal-gas portion of \f$\alpha\f$
    const Eigen::ArrayXd zbulk; ///< The bulk composition of the mixture
    const std::size_t Ncomponents, ///< The number of components in each phase
                      Nphases, ///< The number of phases
                      Nindependent; ///< The number of independent variables to be solved for
    const std::vector<std::shared_ptr<AbstractSpecification>> specifications; ///< The specification equations
    CallResult res; ///< The internal buffer of residual vector and Jacobian (to minimize copies)
    
    /**
     \brief A helper class for doing multi-phase phase equilibrium calculations with additional specification equations
     
     This general approach allows for a generic framework to handle multi-phase equilibrium. The number of phases
     and components are both arbitrary (within the memory available on the machine)
     
     \param residmodel The AbstractModel for the residual portion of the Helmholtz energy
     \param zbulk The bulk molar fractions
     \param init The pack of the initial set of arguments
     \param specifications The two-element vector of specification equations
     
     */
    GeneralizedPhaseEquilibrium(const AbstractModel& residmodel,
                                const Eigen::ArrayXd& zbulk,
                                const UnpackedVariables& init,
                                const std::vector<std::shared_ptr<AbstractSpecification>>& specifications
                                )
    : residptr(residmodel), zbulk(zbulk), Ncomponents(get_Ncomponents(init.rhovecs)), Nphases(init.betas.size()), Nindependent(1+(Ncomponents+1)*Nphases), specifications(specifications)
    {
        if (init.betas.size() != init.rhovecs.size()){
            throw teqp::InvalidArgument("bad sizes for initial betas and rhovecs");
        }
        if (specifications.size() != 2){
            throw teqp::InvalidArgument("specification vector should be of length 2");
        }
        
        // Resize the working buffers
        res.r.resize(Nindependent);
        res.J.resize(Nindependent, Nindependent);
    }
    //    auto attach_ideal_gas(std::unique_ptr<AbstractModel>&ptr){
    //        idealgasptr = ptr;
    //    }
    
    /**
     \brief Call the routines to build the vector of residuals and Jacobian and cache it internally
     \param x The array of independent variables, first T, then molar concentrations of each phase, in order, followed by the molar phase fractions
     */
    auto call(const Eigen::ArrayXd&x){
        // Two references to save some typing later on
        auto& J = res.J; J.setZero();
        auto& r = res.r; r.setZero();
        auto Ncomp = Ncomponents;
        
        // Unpack into the variables of interest (as maps where possible to avoid copies)
        if (x.size() != Nindependent){
            throw teqp::InvalidArgument("Wrong size; should be of size"+ std::to_string(Nindependent) + "; is of size " + std::to_string(x.size()));
        }
        double T = x[0];
        std::vector<Eigen::Map<const Eigen::ArrayXd>> rhovecs;
        for (auto iphase_ = 0; iphase_ < Nphases; ++iphase_){
            rhovecs.push_back(Eigen::Map<const Eigen::ArrayXd>(&x[1 + iphase_*Ncomp], Ncomponents));
        }
        const Eigen::Map<const Eigen::ArrayXd> betas(&x[x.size()-Nphases], Nphases);
        double R = residptr.get_R(zbulk); // TODO: think about what to do when the phases have different R values and dR/drho_i is nonzero
        
        struct RequiredPhaseDerivatives{
            double Psir;
            Eigen::ArrayXd gradient_Psir;
            Eigen::ArrayXXd Hessian_Psir;
            double d_Psir_dT;
            Eigen::ArrayXd d_gradient_Psir_dT;
        };
        // Calculate the required derivatives for each phase
        // based on its temperature and molar concentrations
        auto calculate_required_derivatives = [this, R](double T, const Eigen::ArrayXd& rhovec) -> RequiredPhaseDerivatives{
            RequiredPhaseDerivatives der;
            // Three in one via tuple unpacking
            std::tie(der.Psir, der.gradient_Psir, der.Hessian_Psir) = this->residptr.build_Psir_fgradHessian_autodiff(T, rhovec);
            // And then the temperature derivatives
            // Psir = ar*R*T*rho
            // d(Psir)/dT = d(rho*alphar*R*T)/dT = rho*R*d(alphar*T)/dT = rho*R*(T*dalphar/dT + alphar)
            // and T*dalphar/dT = -Ar10 so
            double rho = rhovec.sum();
            der.d_Psir_dT = rho*R*(-this->residptr.get_Ar10(T, rho, rhovec/rho)) + der.Psir/T;
            der.d_gradient_Psir_dT = this->residptr.build_d2PsirdTdrhoi_autodiff(T, rhovec);
            return der;
        };
        std::vector<RequiredPhaseDerivatives> derivatives;
        for (auto iphase_ = 0; iphase_ < Nphases; ++iphase_){
            derivatives.emplace_back(calculate_required_derivatives(T, rhovecs[iphase_]));
        }
        
        // First we have the equalities in (natural) logarithm of fugacity coefficient (always present)
        std::size_t irow = 0;
        std::size_t iphase = 0;
        
        auto lnf_phase0 = log(rhovecs[iphase]*R*T) + 1/(R*T)*derivatives[iphase].gradient_Psir;
        auto dlnfdT_phase0 = 1/T + 1/(R*T)*derivatives[iphase].d_gradient_Psir_dT - 1/(R*T*T)*derivatives[iphase].gradient_Psir;
        Eigen::ArrayXXd dlnfdrho_phase0 = Eigen::MatrixXd::Identity(Ncomp, Ncomp);
        dlnfdrho_phase0.matrix().diagonal().array() /= rhovecs[iphase];
        dlnfdrho_phase0 += 1/(R*T)*derivatives[iphase].Hessian_Psir;
        
        for (auto iphasei = 1; iphasei < Nphases; ++iphasei){
            // Equality of all ln(f) for phase with index 0 and that of index iphase
            auto lnf_phasei = log(rhovecs[iphasei]*R*T) + 1.0/(R*T)*derivatives[iphasei].gradient_Psir;
            auto dlnfdT_phasei = 1/T + 1.0/(R*T)*derivatives[iphasei].d_gradient_Psir_dT - 1/(R*T*T)*derivatives[iphasei].gradient_Psir;
            Eigen::ArrayXXd dlnfdrho_phasei = Eigen::MatrixXd::Identity(Ncomp, Ncomp);
            dlnfdrho_phasei.matrix().diagonal().array() /= rhovecs[iphasei];
            dlnfdrho_phasei += 1/(R*T)*derivatives[iphasei].Hessian_Psir;
            
            // There are Ncomp equalities for this phase for each component
            r.segment(irow, Ncomp) = lnf_phase0 - lnf_phasei;
            // And Ncomp entries in the first column (of index 0) in the Jacobian for the
            // temperature derivative
            J.block(irow, 0, Ncomp, 1) = dlnfdT_phase0 - dlnfdT_phasei;
            // And in the rows in the Jacobian, there is a block for each phase,
            // including the first one with index 0, and the correct block needs to be selected
            // which for first phase is of positive sign, and elsewhere, of negative sign
            for (auto iphasej = 0; iphasej < Ncomp; ++iphasej){
                if (iphasej == 0){
                    J.block(irow, 1+iphasej*Ncomp, Ncomp, Ncomp) = dlnfdrho_phase0;
                }
                else{
                    J.block(irow, 1+iphasej*Ncomp, Ncomp, Ncomp) = -dlnfdrho_phasei;
                }
            }
            irow += Ncomp;
        }
        
        // Then we have the equality of pressure between all the phases (always present)
        iphase = 0;
        double p_phase0 = rhovecs[iphase].sum()*R*T - derivatives[iphase].Psir + (rhovecs[iphase]*derivatives[iphase].gradient_Psir).sum();
        double dpdT_phase0 = rhovecs[iphase].sum()*R - derivatives[iphase].d_Psir_dT + (rhovecs[iphase]*derivatives[iphase].d_gradient_Psir_dT).sum();
        Eigen::ArrayXd dpdrho_phase0 = (R*T + (rhovecs[iphase].matrix().transpose()*derivatives[iphase].Hessian_Psir.matrix()).array()).eval();
        for (auto iphasei = 1; iphasei < Nphases; ++iphasei){
            double p_phasei = rhovecs[iphasei].sum()*R*T - derivatives[iphasei].Psir + (rhovecs[iphasei]*derivatives[iphasei].gradient_Psir).sum();
            double dpdT_phasei = rhovecs[iphasei].sum()*R - derivatives[iphasei].d_Psir_dT + (rhovecs[iphasei]*derivatives[iphasei].d_gradient_Psir_dT).sum();
            Eigen::ArrayXd dpdrho_phasei = (R*T + (rhovecs[iphasei].matrix().transpose()*derivatives[iphasei].Hessian_Psir.matrix()).array()).eval();
            r[irow] = p_phase0 - p_phasei;
            J(irow, 0) = dpdT_phase0 - dpdT_phasei;
            for (auto iphasej = 0; iphasej < Nphases; ++iphasej){
                if (iphasej == 0){
                    J.block(irow, 1+iphasej*Ncomp, 1, Ncomp) = dpdrho_phase0.transpose();
                }
                else{
                    J.block(irow, 1+iphasej*Ncomp, 1, Ncomp) = -dpdrho_phasei.transpose();
                }
            }
            // Note: no Jacobian contribution for derivatives w.r.t. betas
            irow += 1;
        }
        
        // Then we have the Ncomp-1 material balances (always present)
        for (auto icomp = 0; icomp < Ncomp-1; ++icomp){
            double summer = 0;
            for (iphase = 0; iphase < Nphases; ++iphase){
                double rho_phase = rhovecs[iphase].sum();
                double xj_phk = rhovecs[iphase][icomp]/rho_phase; // mole fraction of component j in phase k
                summer += betas[iphase]*xj_phk;
                J(irow, J.cols()-Nphases+iphase) = xj_phk;
                for (auto icompj = 0; icompj < Ncomp; ++icompj){
                    auto Kronecker = [](int i, int j){ return i==j; };
                    J(irow, 1+iphase*Ncomp + icompj) = betas[iphase]*(Kronecker(icomp,icompj) - rhovecs[iphase][icomp]/rho_phase)/rho_phase;
                }
            }
            r[irow] = summer - zbulk[icomp];
            irow++;
        }
        
        // Summation of molar phase fractions beta (always present)
        r[irow] = betas.sum()-1;
        J.block(irow, Nindependent-Nphases, 1, Nphases).fill(1.0); // All other derivatives zero
        irow += 1;
        
        // And two specification equations
        SpecificationSidecar sidecar;
        sidecar.Nphases = Nphases;
        sidecar.Ncomponents = Ncomponents;
        sidecar.Nindependent = Nindependent;
        sidecar.ptr_p_phase0 = &p_phase0;
        sidecar.ptr_dpdT_phase0 = &dpdT_phase0;
        sidecar.ptr_dpdrho_phase0 = &dpdrho_phase0;
        for (auto& spec: specifications){
            auto [r_, J_] = spec->r_Jacobian(x, sidecar);
            r[irow] = r_;
            J.row(irow) = J_;
            irow += 1;
        }
        
        // Finally, zero out the rows and columns in the Jacobian where mole fractions are zero, which would otherwise cause issues
    }
    auto num_Jacobian(const Eigen::ArrayXd& x, const Eigen::ArrayXd& dx){
        Eigen::MatrixXd J(Nindependent, Nindependent);
        call(x);
        Eigen::ArrayXd r0 = res.r;
        for (auto i = 0; i < Nindependent; ++i){
            double dx_ = (dx[i] != 0) ? dx[i] : 1e-6;
            Eigen::ArrayXd xplus = x; xplus[i] += dx_;
            Eigen::ArrayXd xminus = x; xminus[i] -= dx_;
            call(xplus);
            auto rplus = res.r;
            call(xminus);
            auto rminus = res.r;
            J.col(i) = (rplus - rminus)/(2*dx_);
        }
        return J;
    }
};

}
