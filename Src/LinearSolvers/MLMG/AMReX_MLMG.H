#ifndef AMREX_ML_MG_H_
#define AMREX_ML_MG_H_
#include <AMReX_Config.H>

#include <AMReX_MLLinOp.H>
#include <AMReX_iMultiFab.H>
#include <AMReX_MLCGSolver.H>

#if defined(AMREX_USE_HYPRE) && (AMREX_SPACEDIM > 1)
#include <AMReX_Hypre.H>
#include <AMReX_HypreNodeLap.H>
#endif

namespace amrex {

#ifdef AMREX_USE_PETSC
class PETScABecLap;
#endif

class MLMG
{
public:

    friend class MLCGSolver;

    using BCMode = MLLinOp::BCMode;
    using Location = MLLinOp::Location;

    using BottomSolver = amrex::BottomSolver;
    enum class CFStrategy : int {none,ghostnodes};

    MLMG (MLLinOp& a_lp);
    ~MLMG ();

    // Optional argument checkpoint_file is for debugging only.
    Real solve (const Vector<MultiFab*>& a_sol, const Vector<MultiFab const*>& a_rhs,
                Real a_tol_rel, Real a_tol_abs, const char* checkpoint_file = nullptr);

    // For this version of solve, Any holds MultiFab like objects.
    Real solve (Vector<Any>& a_sol, const Vector<Any>& a_rhs,
                Real a_tol_rel, Real a_tol_abs, const char* checkpoint_file = nullptr);

    void getGradSolution (const Vector<Array<MultiFab*,AMREX_SPACEDIM> >& a_grad_sol,
                          Location a_loc = Location::FaceCenter);

    /**
    * \brief For ``(alpha * a - beta * (del dot b grad)) phi = rhs``, flux means ``-b grad phi``
    *
    * \param a_flux
    * \param a_loc
    */
    void getFluxes (const Vector<Array<MultiFab*,AMREX_SPACEDIM> >& a_flux,
                    Location a_loc = Location::FaceCenter);
    void getFluxes (const Vector<Array<MultiFab*,AMREX_SPACEDIM> >& a_flux,
                    const Vector<MultiFab*> & a_sol,
                    Location a_loc = Location::FaceCenter);
    void getFluxes (const Vector<MultiFab*> & a_flux, Location a_loc = Location::CellCenter);
    void getFluxes (const Vector<MultiFab*> & a_flux, const Vector<MultiFab*> & a_sol, Location a_loc = Location::CellCenter);
    void compResidual (const Vector<MultiFab*>& a_res, const Vector<MultiFab*>& a_sol,
                       const Vector<MultiFab const*>& a_rhs);

#ifdef AMREX_USE_EB
    // Flux into the EB wall
    void getEBFluxes (const Vector<MultiFab*>& a_eb_flux);
    void getEBFluxes (const Vector<MultiFab*>& a_eb_flux, const Vector<MultiFab*> & a_sol);
#endif

    /**
    * \brief ``out = L(in)``. Note that, if no actual solve is needed, one could
    * turn off multigrid coarsening by constructing a MLLinOp object
    * with an appropriate LPInfo object (e.g., with LPInfo().setMaxCoarseningLevel(0)).
    *
    * \param out
    * \param in
    */
    void apply (const Vector<MultiFab*>& out, const Vector<MultiFab*>& in);

    void setVerbose (int v) noexcept { verbose = v; }
    void setMaxIter (int n) noexcept { max_iters = n; }
    void setMaxFmgIter (int n) noexcept { max_fmg_iters = n; }
    void setFixedIter (int nit) noexcept { do_fixed_number_of_iters = nit; }

    void setPreSmooth (int n) noexcept { nu1 = n; }
    void setPostSmooth (int n) noexcept { nu2 = n; }
    void setFinalSmooth (int n) noexcept { nuf = n; }
    void setBottomSmooth (int n) noexcept { nub = n; }

    void setBottomSolver (BottomSolver s) noexcept { bottom_solver = s; }
    void setCFStrategy (CFStrategy a_cf_strategy) noexcept {cf_strategy = a_cf_strategy;}
    void setBottomVerbose (int v) noexcept { bottom_verbose = v; }
    void setBottomMaxIter (int n) noexcept { bottom_maxiter = n; }
    void setBottomTolerance (Real t) noexcept { bottom_reltol = t; }
    void setBottomToleranceAbs (Real t) noexcept { bottom_abstol = t;}
    Real getBottomToleranceAbs () noexcept{ return bottom_abstol; }

    void setAlwaysUseBNorm (int flag) noexcept { always_use_bnorm = flag; }

    void setFinalFillBC (int flag) noexcept { final_fill_bc = flag; }

    int numAMRLevels () const noexcept { return namrlevs; }

    void setNSolve (int flag) noexcept { do_nsolve = flag; }
    void setNSolveGridSize (int s) noexcept { nsolve_grid_size = s; }

#if defined(AMREX_USE_HYPRE) && (AMREX_SPACEDIM > 1)
    void setHypreInterface (Hypre::Interface f) noexcept {
        // must use ij interface for EB
#ifndef AMREX_USE_EB
        hypre_interface = f;
#else
        amrex::ignore_unused(f);
#endif
    }

    //! Set the namespace in input file for parsing HYPRE specific options
    void setHypreOptionsNamespace(const std::string& prefix) noexcept
    {
        hypre_options_namespace = prefix;
    }

    void setHypreOldDefault (bool l) noexcept {hypre_old_default = l;}
    void setHypreRelaxType (int n) noexcept {hypre_relax_type = n;}
    void setHypreRelaxOrder (int n) noexcept {hypre_relax_order = n;}
    void setHypreNumSweeps (int n) noexcept {hypre_num_sweeps = n;}
    void setHypreStrongThreshold (Real t) noexcept {hypre_strong_threshold = t;}
#endif

    void prepareForSolve (Vector<Any>& a_sol, const Vector<Any>& a_rhs);

    void prepareForNSolve ();

    void oneIter (int iter);

    void miniCycle (int alev);

    void mgVcycle (int amrlev, int mglev);
    void mgFcycle ();

    void bottomSolve ();
    void NSolve (MLMG& a_solver, MultiFab& a_sol, MultiFab& a_rhs);
    void actualBottomSolve ();

    void computeMLResidual (int amrlevmax);
    void computeResidual (int alev);
    void computeResWithCrseSolFineCor (int crse_amr_lev, int fine_amr_lev);
    void computeResWithCrseCorFineCor (int fine_amr_lev);
    void interpCorrection (int alev);
    void interpCorrection (int alev, int mglev);
    void addInterpCorrection (int alev, int mglev);

    void computeResOfCorrection (int amrlev, int mglev);

    Real ResNormInf (int amrlev, bool local = false);
    Real MLResNormInf (int alevmax, bool local = false);
    Real MLRhsNormInf (bool local = false);
    void buildFineMask ();

    void makeSolvable ();
    void makeSolvable (int amrlev, int mglev, Any& mf);

#if defined(AMREX_USE_HYPRE) && (AMREX_SPACEDIM > 1)
    void bottomSolveWithHypre (Any& x, const Any& b);
#endif

    void bottomSolveWithPETSc (Any& x, const Any& b);

    int bottomSolveWithCG (Any& x, const Any& b, MLCGSolver::Type type);

    Real getInitRHS () const noexcept { return m_rhsnorm0; }
    // Initial composite residual
    Real getInitResidual () const noexcept { return m_init_resnorm0; }
    // Final composite residual
    Real getFinalResidual () const noexcept { return m_final_resnorm0; }
    // Residuals on the *finest* AMR level after each iteration
    Vector<Real> const& getResidualHistory () const noexcept { return m_iter_fine_resnorm0; }
    int getNumIters () const noexcept { return m_iter_fine_resnorm0.size(); }
    Vector<int> const& getNumCGIters () const noexcept { return m_niters_cg; }

private:

    int verbose = 1;
    int max_iters = 200;
    int do_fixed_number_of_iters = 0;

    int nu1 = 2;       //!< pre
    int nu2 = 2;       //!< post
    int nuf = 8;       //!< when smoother is used as bottom solver
    int nub = 0;       //!< additional smoothing after bottom cg solver

    int max_fmg_iters = 0;

    BottomSolver bottom_solver = BottomSolver::Default;
    CFStrategy cf_strategy     = CFStrategy::none;
    int  bottom_verbose        = 0;
    int  bottom_maxiter        = 200;
    Real bottom_reltol         = Real(1.e-4);
    Real bottom_abstol         = Real(-1.0);

    int always_use_bnorm = 0;

    int final_fill_bc = 0;

    MLLinOp& linop;
    int namrlevs;
    int finest_amr_lev;

    bool linop_prepared = false;
    Long solve_called = 0;

    //! N Solve
    int do_nsolve = false;
    int nsolve_grid_size = 16;
    std::unique_ptr<MLLinOp> ns_linop;
    std::unique_ptr<MLMG> ns_mlmg;
    std::unique_ptr<MultiFab> ns_sol;
    std::unique_ptr<MultiFab> ns_rhs;

    //! Hypre
#if defined(AMREX_USE_HYPRE) && (AMREX_SPACEDIM > 1)
    // Hypre::Interface hypre_interface = Hypre::Interface::structed;
    // Hypre::Interface hypre_interface = Hypre::Interface::semi_structed;
    Hypre::Interface hypre_interface = Hypre::Interface::ij;

    std::unique_ptr<Hypre> hypre_solver;
    std::unique_ptr<MLMGBndry> hypre_bndry;
    std::unique_ptr<HypreNodeLap> hypre_node_solver;

    std::string hypre_options_namespace = "hypre";
    bool hypre_old_default = true; // Falgout coarsening with modified classical interpolation
    int hypre_relax_type = 6;  // G-S/Jacobi hybrid relaxation
    int hypre_relax_order = 1; // uses C/F relaxation
    int hypre_num_sweeps = 2;  // Sweeps on each level
    Real hypre_strong_threshold = 0.25; // Hypre default is 0.25
#endif

    //! PETSc
#ifdef AMREX_USE_PETSC
    std::unique_ptr<PETScABecLap> petsc_solver;
    std::unique_ptr<MLMGBndry> petsc_bndry;
#endif

    /**
    * \brief To avoid confusion, terms like sol, cor, rhs, res, ... etc. are
    * in the frame of the original equation, not the correction form
    */
    Vector<Any> sol;      //!< Might be alias to argument a_sol
    Vector<Any> rhs;      //!< Copy of original rhs
                          //! L(sol) = rhs

    Vector<int> sol_is_alias;

    /**
    * \brief First Vector: Amr levels.  0 is the coarest level
    * Second Vector: MG levels.  0 is the finest level
    */
    Vector<Vector<Any> > res;     //! = rhs - L(sol)
    Vector<Vector<Any> > cor;     //!< L(cor) = res
    Vector<Vector<Any> > cor_hold;
    Vector<Vector<Any> > rescor;  //!< = res - L(cor)
                                  //!  Residual of the correction form

    enum timer_types { solve_time=0, iter_time, bottom_time, ntimers };
    Vector<double> timer;

    Real m_rhsnorm0 = -1.0;
    Real m_init_resnorm0 = -1.0;
    Real m_final_resnorm0 = -1.0;
    Vector<int> m_niters_cg;
    Vector<Real> m_iter_fine_resnorm0; // Residual for each iteration at the finest level

    void checkPoint (const Vector<MultiFab*>& a_sol, const Vector<MultiFab const*>& a_rhs,
                     Real a_tol_rel, Real a_tol_abs, const char* a_file_name) const;
};

}

#endif
